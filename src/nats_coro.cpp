#include "nats_coro.h"
#include "connect_to_nats.h"
#include "queue.h"
#include "subscribe.h"

#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/use_coro.hpp>
#include <boost/asio/detached.hpp>

#include <boost/noncopyable.hpp>

#include <format>
#include <optional>
#include <map>
#include <iterator>

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

private:
    ip::tcp::socket _socket;

    std::atomic_size_t _subscribes_count;

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
};

awaitable<std::shared_ptr<IClient>> createClient(std::string_view url) {
    auto socket = co_await connect_to_nats(url);
    co_return std::make_shared<Client>(std::move(socket));
}

Client::Client(ip::tcp::socket&& socket)
    :
    _socket{std::move(socket)},
    _subscribes_count{0}
{
    auto [front, back] = make_queue_mp<TXMessage>(64);
    _txQueueFront.emplace(std::move(front));
    _txQueueBack.emplace(std::move(back));
}

awaitable<void> Client::publish(std::string_view subject,
                                std::string_view payload)
{
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

Unsub Client::make_unsub(TXMessage&& txMsg)
{
    auto push_unsub = [this,
                       txMsg = std::move(txMsg)]() mutable -> awaitable<void>
    {
        co_await _txQueueFront->push(std::move(txMsg));
    };

    return Unsub{std::move(push_unsub)};
}

awaitable<void> Client::run()
{
    co_await (
        rx() ||
        tx(std::move(*_txQueueBack))
    );
}

awaitable<void> Client::rx()
{
    using namespace std::string_view_literals;

    for (;;) {
        std::string buffer;
        std::size_t controlLineSize = co_await async_read_until(_socket,
                                                                dynamic_buffer(buffer),
                                                                "\r\n"sv,
                                                                use_awaitable);
        if (buffer.starts_with("MSG"sv))
        {
            ControlLineView head = parse_msg(buffer);
            std::size_t totalMessageSize = buffer.size() + head.payload_size() + 2; // + \r\n
            buffer.reserve(totalMessageSize);

            std::size_t payloadLineSize = co_await async_read_until(_socket,
                                                                    dynamic_buffer(buffer),
                                                                    "\r\n"sv,
                                                                    use_awaitable);

            auto payload = std::make_pair(controlLineSize,
                                          std::min(head.payload_size(),
                                                   payloadLineSize - 2)); // without \r\n
            assert(payload.second == head.payload_size());

            // Add comporator for lookup by string_view
            // https://stackoverflow.com/questions/69678864/safe-way-to-use-string-view-as-key-in-unordered-map
            if (auto it = _subscribes.find(std::string{head.subscribe_id()});
                it != std::end(_subscribes))
            {
                auto& queueFront = it->second;
                co_await queueFront.push(Message{std::move(buffer), head, payload});
            }


            //co_await _subscribes->push(Message{std::move(buffer), head, payload});
        }
        else if (buffer.starts_with("PING"sv))
        {
            co_await pong();
        }
    }
}

awaitable<void> Client::tx(TXQueueBack txQueueBack)
{
    for (;;) {
        auto range = co_await txQueueBack.get();
        for (std::size_t seq : range)
        {
            TXMessage& msg = txQueueBack[seq];
            msg.before_send();
            co_await async_write(_socket,
                                 buffer(msg.content),
                                 use_awaitable);
            co_await msg.after_send();
        }
        txQueueBack.consume(range);
    }
}

awaitable<void> Client::pong()
{
    co_await _txQueueFront->push(TXMessage{"PONG\r\n"});
}

std::string Client::generate_subscribe_id()
{
    return std::to_string(_subscribes_count.fetch_add(1, std::memory_order_relaxed));
}

} // namespace nats_coro
