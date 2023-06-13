#include "nats_coro.h"
#include "connect_to_nats.h"
#include "queue.h"
#include "subscribe.h"
#include "received.h"

#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/use_coro.hpp>
#include <boost/asio/detached.hpp>

#include <boost/noncopyable.hpp>

#include <format>
#include <optional>
#include <map>
#include <iterator>

#include <iostream> // tmp

namespace nats_coro {

using namespace experimental::awaitable_operators;
using experimental::use_coro;

struct TXMessage
{
    std::string content;

    void before_send() { if (_before_send) std::move(_before_send)(); }
    awaitable<void> after_send() { if (_after_send) co_await std::move(_after_send)(); }

    std::move_only_function<void()> _before_send;
    std::move_only_function<awaitable<void>()> _after_send;
};

using TXQueueFront = std::decay_t<decltype(std::get<0>(make_queue_mp<TXMessage>(0)))>;
using TXQueueBack = std::decay_t<decltype(std::get<1>(make_queue_mp<TXMessage>(0)))>;

class Client : public IClient, boost::noncopyable
{
public:
    Client(ip::tcp::socket&& socket);

    awaitable<void> run() override;

    awaitable<void> publish(std::string_view subject,
                            std::string_view payload) override;

    awaitable<Subscribe> subscribe(std::string_view subject) override;

    awaitable<void> shutdown() override;

private:
    ip::tcp::socket _socket;

    std::atomic_size_t _subscribe_id_counter;
    std::atomic_bool _isShutdown;

    std::optional<TXQueueFront> _txQueueFront;
    std::optional<TXQueueBack> _txQueueBack;

    std::map<std::string, SubQueueFront> _subscribes;

    awaitable<void> rx();
    awaitable<void> tx(TXQueueBack);

    awaitable<void> pong();

    std::string generate_subscribe_id();
    TXMessage make_sub_tx_message(std::string_view subject,
                                  std::string subId,
                                  SubQueueFront&& queueFront);
    TXMessage make_unsub_tx_message(std::string subId);
    Unsub make_unsub(TXMessage&&);
    TXMessage make_shutdown_tx_message();
};

awaitable<std::shared_ptr<IClient>> createClient(std::string_view url) {
    auto socket = co_await connect_to_nats(url);
    co_return std::make_shared<Client>(std::move(socket));
}

Client::Client(ip::tcp::socket&& socket)
    :
    _socket{std::move(socket)},
    _subscribe_id_counter{0},
    _isShutdown{false}
{
    auto [front, back] = make_queue_mp<TXMessage>(64);
    _txQueueFront.emplace(std::move(front));
    _txQueueBack.emplace(std::move(back));
}

awaitable<void> Client::publish(std::string_view subject,
                                std::string_view payload)
{
    if (_isShutdown.load(std::memory_order_acquire)) {
        throw boost::system::system_error{error::operation_aborted};
    }
    std::string content = std::format("PUB {} {}\r\n{}\r\n",
                                      subject,
                                      payload.size(),
                                      payload);
    co_await _txQueueFront->push(TXMessage{std::move(content)});
}

awaitable<IClient::Subscribe> Client::subscribe(std::string_view subject)
{
    std::string subId = generate_subscribe_id();
    auto [front, back] = make_queue_sp<Message>(64);

    TXMessage subMsg = make_sub_tx_message(subject,
                                           subId,
                                           std::move(front));
    TXMessage unsubMsg = make_unsub_tx_message(subId);

    coro<Message> sub = subscription(co_await this_coro::executor,
                                     std::move(back));
    Unsub unsub = make_unsub(std::move(unsubMsg));

    co_await _txQueueFront->push(std::move(subMsg));
    co_return std::make_tuple(std::move(sub), std::move(unsub));
}

TXMessage Client::make_sub_tx_message(std::string_view subject,
                                      std::string subId,
                                      SubQueueFront&& queueFront)
{
    return {.content = std::format("SUB {} {}\r\n",
                                   subject,
                                   subId),
            ._before_send = [this,
                             subId = std::move(subId),
                             queueFront = std::move(queueFront)]() mutable
            {
                assert(!_subscribes.contains(subId));
                _subscribes.emplace(std::move(subId), std::move(queueFront));
            }
    };
}

TXMessage Client::make_unsub_tx_message(std::string subId)
{
    return {.content = std::format("UNSUB {}\r\n",
                                   subId),
            ._after_send = [this,
                            subId = std::move(subId)]() -> awaitable<void>
            {
                assert(_subscribes.contains(subId));
                try {
                    SubQueueFront& queue = _subscribes.find(subId)->second;
                    co_await queue.push(Message{}); // push EOF
                } catch (const boost::system::system_error& ex) {
                    // queue back maybe destroyed
                    assert(ex.code() == error::operation_aborted);
                }
                _subscribes.erase(subId);
            }
    };
}

struct Unsub::Impl
{
    auto push() -> awaitable<void>
    {
        bool isFirst = !std::exchange(_pushed, true);
        if (isFirst) {
            co_await _txQueue.push(std::move(_msg));
        }
    }

    ~Impl()
    {
        if (_pushed) {
            return;
        }
        try {
            auto push = [](TXQueueFront& queue, TXMessage msg) -> awaitable<void>
            {
                co_await queue.push(std::move(msg));
            };
            co_spawn(_executor,
                     push(_txQueue, std::move(_msg)),
                     detached);
        }
        catch (...) {}
    }

    TXMessage _msg;
    TXQueueFront& _txQueue;
    any_io_executor _executor;

    bool _pushed = false;
};

awaitable<void> Unsub::operator()()
{
    return _impl->push();
}

Unsub Client::make_unsub(TXMessage&& txMsg)
{
    auto impl = std::make_shared<Unsub::Impl>(std::move(txMsg),
                                              *_txQueueFront,
                                              _socket.get_executor());
    return Unsub{std::move(impl)};
}

awaitable<void> Client::run()
{
    assert(co_await this_coro::executor == _socket.get_executor());
    co_await (
        rx() ||
        tx(std::move(*_txQueueBack))
    );
}

awaitable<void> Client::rx()
{
    using namespace std::string_view_literals;
    streambuf buffer;

    for (;;) {
        std::size_t readed = co_await async_read_until(_socket,
                                                       buffer,
                                                       "\r\n"sv,
                                                       use_awaitable);
        // A single contiguous character array
        BOOST_ASIO_CONST_BUFFER sb = buffer.data();
        auto controlLine = std::string_view(static_cast<const char*>(sb.data()),
                                            readed);

        if (controlLine.starts_with("MSG"sv))
        {
            ControlLineView head = parse_msg(controlLine);
            std::size_t payloadOffset = controlLine.size();
            std::size_t totalMsgSize = payloadOffset +
                                       head.payload_size() +
                                       "\r\n"sv.size();

            readed = co_await async_read_until(_socket,
                                               buffer,
                                               received(totalMsgSize),
                                               use_awaitable);

            auto payload = std::make_pair(payloadOffset,
                                          head.payload_size());

            auto begin = buffers_begin(buffer.data());
            auto data = std::string(begin,
                                    begin + totalMsgSize);

            // Add comporator for lookup by string_view
            // https://stackoverflow.com/questions/69678864/safe-way-to-use-string-view-as-key-in-unordered-map
            if (auto it = _subscribes.find(std::string{head.subscribe_id()});
                it != std::end(_subscribes))
            {
                auto& queue = it->second;
                co_await queue.push(Message{std::move(data), head, payload});
            }
        }
        else if (controlLine.starts_with("PING"sv))
        {
            co_await pong();
        }

        buffer.consume(readed);
    }
}

awaitable<void> Client::tx(TXQueueBack txQueueBack)
{
    bool isStopping = false;
    do {
        auto range = co_await txQueueBack.get();
        for (std::size_t seq : range)
        {
            TXMessage& msg = txQueueBack[seq];
            msg.before_send();
            if (msg.content.empty()) {
                isStopping = true;
            } else {
                co_await async_write(_socket,
                                     buffer(msg.content),
                                     use_awaitable);
            }
            co_await msg.after_send();
        }
        txQueueBack.consume(range);
    } while (!isStopping ||
             !_subscribes.empty());
}

awaitable<void> Client::pong()
{
    co_await _txQueueFront->push(TXMessage{"PONG\r\n"});
}

std::string Client::generate_subscribe_id()
{
    return std::to_string(_subscribe_id_counter.fetch_add(1, std::memory_order_relaxed));
}

TXMessage Client::make_shutdown_tx_message()
{
    return {._after_send = [this]() -> awaitable<void>
    {
        for (auto& [_, queue] : _subscribes)
        {
            try {
                co_await queue.push(Message{}); // push EOF
            } catch (const boost::system::system_error& ex) {
                // queue back maybe destroyed
                assert(ex.code() == error::operation_aborted);
            }
        }
    }
    };
}

awaitable<void> Client::shutdown()
{
    bool isAlready = _isShutdown.exchange(true, std::memory_order_acquire);
    if (isAlready) {
        co_return;
    }
    co_await _txQueueFront->push(make_shutdown_tx_message());
}

} // namespace nats_coro
