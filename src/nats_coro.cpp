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
using experimental::make_parallel_group;
using experimental::wait_for_one;
using experimental::wait_for_one_error;

struct TXMessage
{
    std::string content;

    void before_send() { if (_before_send) std::move(_before_send)(); }
    awaitable<void> after_send() { if (_after_send) co_await std::move(_after_send)(); }

    std::move_only_function<void()> _before_send;
    std::move_only_function<awaitable<void>()> _after_send;
};

using TXQueueHead = std::decay_t<decltype(std::get<0>(make_queue_mp<TXMessage>(0)))>;
using TXQueueTail = std::decay_t<decltype(std::get<1>(make_queue_mp<TXMessage>(0)))>;

class Client : public IClient, boost::noncopyable
{
public:
    Client(ip::tcp::socket&& socket);

    awaitable<void> run() override;

    awaitable<void> publish(std::string_view subject,
                            std::string_view payload) override;

    awaitable<Subscribe> subscribe(any_io_executor executor,
                                   std::string_view subject) override;

    awaitable<void> shutdown() override;

private:
    ip::tcp::socket _socket;

    std::atomic_size_t _subscribe_id_counter;
    std::atomic_bool _isShutdown;
    awaitable_ext::Event _shutdownEvent;

    std::optional<TXQueueHead> _txQueueHead;
    std::optional<TXQueueTail> _txQueueTail;
    
    std::map<std::string, SubQueueTail> _subscribes;

    awaitable<void> rx();
    awaitable<void> tx(TXQueueHead);

    awaitable<void> pong();

    std::string generate_subscribe_id();
    TXMessage make_sub_tx_message(std::string_view subject,
                                  std::string subId,
                                  SubQueueTail&& queue);
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
    auto [head, tail] = make_queue_mp<TXMessage>(64);
    _txQueueHead.emplace(std::move(head));
    _txQueueTail.emplace(std::move(tail));
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
    co_await _txQueueTail->push(TXMessage{std::move(content)});
}

awaitable<IClient::Subscribe> Client::subscribe(any_io_executor executor,
                                                std::string_view subject)
{
    if (_isShutdown.load(std::memory_order_acquire)) {
        throw boost::system::system_error{error::operation_aborted};
    }
    std::string subId = generate_subscribe_id();
    auto [queueHead, queueTail] = make_queue_sp<Message>(64);

    TXMessage subMsg = make_sub_tx_message(subject,
                                           subId,
                                           std::move(queueTail));
    TXMessage unsubMsg = make_unsub_tx_message(subId);

    coro<Message> sub = subscription(executor,
                                     std::move(queueHead));
    Unsub unsub = make_unsub(std::move(unsubMsg));

    co_await _txQueueTail->push(std::move(subMsg));
    co_return std::make_tuple(std::move(sub), std::move(unsub));
}

TXMessage Client::make_sub_tx_message(std::string_view subject,
                                      std::string subId,
                                      SubQueueTail&& queue)
{
    return {.content = std::format("SUB {} {}\r\n",
                                   subject,
                                   subId),
            ._before_send = [this,
                             subId = std::move(subId),
                             queue = std::move(queue)]() mutable
            {
                assert(!_subscribes.contains(subId));
                _subscribes.emplace(std::move(subId), std::move(queue));
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
                try {
                    assert(_subscribes.contains(subId));
                    SubQueueTail& queue = _subscribes.find(subId)->second;
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
        auto msg = std::exchange(_msg, std::nullopt);
        if (msg) {
            co_await _txQueue.push(std::move(*msg));
        }
    }

    ~Impl()
    {
        if (!_msg) {
            return;
        }
        try {
            auto push = [](TXQueueTail& queue, TXMessage msg) -> awaitable<void>
            {
                co_await queue.push(std::move(msg));
            };
            co_spawn(_executor,
                     push(_txQueue, std::move(*_msg)),
                     detached);
        }
        catch (...) {}
    }

    std::optional<TXMessage> _msg;
    TXQueueTail& _txQueue;
    any_io_executor _executor;
};

awaitable<void> Unsub::operator()()
{
    return _impl->push();
}

Unsub Client::make_unsub(TXMessage&& txMsg)
{
    auto impl = std::make_shared<Unsub::Impl>(std::move(txMsg),
                                              *_txQueueTail,
                                              _socket.get_executor());
    return Unsub{std::move(impl)};
}

awaitable<void> Client::run()
{
    if (!_socket.is_open()) {
        throw boost::system::system_error{error::operation_aborted};
    }
    auto executor = _socket.get_executor();

    auto rxWrap = [this]() -> awaitable<void>
    {
        auto executor = co_await this_coro::executor;
        auto [order, rxEx, shutdownEc] = co_await make_parallel_group(
            co_spawn(executor, rx(), deferred),
            _shutdownEvent.wait(deferred))
                                             .async_wait(wait_for_one(), deferred);

        if (order[0] == 0) {
            assert(!!rxEx); // rx() first only with net error
            std::rethrow_exception(rxEx);
        }
    };

    auto [order, rxEx, txEx] = co_await make_parallel_group(
        co_spawn(executor, rxWrap(), deferred),
                                   co_spawn(executor, tx(std::move(*_txQueueHead)), deferred))
                                   .async_wait(wait_for_one_error(), deferred);

    {
        boost::system::error_code dummy;
        _socket.shutdown(ip::tcp::socket::shutdown_both, dummy);
        _socket.close();
    }

    for (auto& [_, queue] : _subscribes)
    {
        try {
            co_await queue.push(Message{}); // push EOF
        } catch (const boost::system::system_error& ex) {
            // queue back maybe destroyed
            assert(ex.code() == error::operation_aborted);
        }
    }

    if (order[0] == 0 && rxEx) {
        std::rethrow_exception(rxEx);
    }
    if (order[0] == 1 && txEx) {
        std::rethrow_exception(txEx);
    }
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
            assert(readed == totalMsgSize);

            auto begin = buffers_begin(buffer.data());
            auto data = std::string(begin,
                                    begin + totalMsgSize);
            assert(data.ends_with("\r\n"));
            auto payload = std::make_pair(payloadOffset,
                                          head.payload_size());
            auto msg = Message(std::move(data), head, payload);

            // From msg because head storage maybe not valid
            auto subId = msg.head().subscribe_id();
            // Add comporator for lookup by string_view
            // https://stackoverflow.com/questions/69678864/safe-way-to-use-string-view-as-key-in-unordered-map
            if (auto it = _subscribes.find(std::string{subId});
                it != std::end(_subscribes))
            {
                auto& queue = it->second;
                co_await queue.push(std::move(msg));
            }
        }
        else if (controlLine.starts_with("PING"sv))
        {
            co_await pong();
        }
        else if (controlLine.starts_with("-ERR")) {
            throw boost::system::system_error{error::eof, std::string(controlLine)};
        }

        buffer.consume(readed);
    }
}

awaitable<void> Client::tx(TXQueueHead txQueueBack)
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
    co_await _txQueueTail->push(TXMessage{"PONG\r\n"});
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
    _shutdownEvent.set();
    co_await _txQueueTail->push(make_shutdown_tx_message());
}

} // namespace nats_coro
