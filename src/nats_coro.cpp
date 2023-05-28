#include "nats_coro.h"
#include "connect_to_nats.h"
#include "queue.h"

#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/noncopyable.hpp>

#include <format>
#include <optional>

namespace nats_coro {

class Client : public IClient, boost::noncopyable
{
public:
    Client(ip::tcp::socket socket, std::size_t txBufferSize = 64) : _socket{std::move(socket)} {}

    awaitable<void> run() override;

    awaitable<void> publish(std::string_view subject,
                            std::string_view payload) override;

private:
    ip::tcp::socket _socket;

    using TXQueueFront = std::decay_t<decltype(std::get<0>(make_queue_mp<std::string>(0)))>;
    using TXQueueBack = std::decay_t<decltype(std::get<1>(make_queue_mp<std::string>(0)))>;

    std::optional<TXQueueFront> _txQueueFront;

    awaitable<void> rx();
    awaitable<void> tx(TXQueueBack&&);

    awaitable<std::string> get_message();
    awaitable<void> pong();
};

awaitable<std::shared_ptr<IClient>> createClient(std::string_view url) {
    auto socket = co_await connect_to_nats(url);
    co_return std::make_shared<Client>(std::move(socket));
}

using namespace experimental::awaitable_operators;

awaitable<void> Client::run()
{
    auto [front, back] = make_queue_mp<std::string>(64);
    _txQueueFront.emplace(std::move(front));

    co_await (rx() || tx(std::move(back)));
}

awaitable<void> Client::rx()
{
    for (;;) {
        auto msg = co_await get_message();
        if (msg == "PING\r\n") {
            co_await pong();
        }
    }
}

awaitable<void> Client::tx(TXQueueBack&& txQueueBack)
{
    for (;;) {
        awaitable_ext::SequenceRange<std::size_t> range = co_await txQueueBack.get();
        for (std::size_t seq : range) {
            auto buf = buffer(txQueueBack[seq]);
            co_await async_write(_socket, buf, use_awaitable);
        }
        txQueueBack.consume(range);
    }
}

awaitable<void> Client::publish(std::string_view subject,
                                std::string_view payload)
{
    std::string packet = std::format("PUB {} {}\r\n{}\r\n",
                                     subject,
                                     std::to_string(payload.size()),
                                     payload);
    co_await _txQueueFront->push(std::move(packet));
}

awaitable<std::string> Client::get_message()
{
    std::string buf;
    std::size_t size = co_await async_read_until(_socket, dynamic_buffer(buf), "\r\n", use_awaitable);
    buf.resize(size);
    co_return buf;
}

awaitable<void> Client::pong()
{
    co_await _txQueueFront->push(std::string("PONG\r\n"));
}

} // namespace nats_coro
