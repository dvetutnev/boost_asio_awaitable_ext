#include "nats_coro.h"

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/json/value.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/parse.hpp>

#include <format>

namespace nats_coro {

namespace json = boost::json;
using namespace experimental::awaitable_operators;

json::value build_handshake(const json::value& srvConfig, std::string_view token) {
    return {{"auth_token", token}};
}

auto connect_to_nats(std::string_view host,
                     std::string_view port,
                     std::string_view token) -> awaitable<ip::tcp::socket>
{
    auto executor = co_await this_coro::executor;
    auto resolver = ip::tcp::resolver(executor);
    auto endpoints = co_await resolver.async_resolve(host, port, use_awaitable);
    auto socket = ip::tcp::socket(executor);
    co_await async_connect(socket, endpoints, use_awaitable);

    std::string buf;
    co_await async_read_until(socket, dynamic_buffer(buf), "\r\n", use_awaitable);
    auto data = std::string_view(buf);

    auto head = std::string_view("INFO ");
    data.remove_prefix(head.size());
    json::value srvInfo = json::parse(data);

    std::string reply = std::format("CONNECT {}\r\n",
                                    json::serialize(build_handshake(srvInfo,
                                                                    token)));
    co_await async_write(socket, buffer(reply), use_awaitable);

    co_return socket;
}

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
            co_await async_write(_socket, buffer("PONG\r\n"), use_awaitable);
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

awaitable<std::string> Client::get_message()
{
    std::string buf;
    std::size_t size = co_await async_read_until(_socket, dynamic_buffer(buf), "\r\n", use_awaitable);
    buf.resize(size);
    co_return buf;
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

} // namespace nats_coro
