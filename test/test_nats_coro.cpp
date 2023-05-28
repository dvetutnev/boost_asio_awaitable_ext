#include "nats_coro.h"
#include "event.h"
#include "utils.h"
#include "connect_to_nats.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/test/unit_test.hpp>

namespace nats_coro::test {

using namespace boost::asio::awaitable_ext;
using namespace boost::asio::experimental::awaitable_operators;

using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(nats_coro);

namespace {

auto mock_nats(unsigned port) -> awaitable<ip::tcp::socket>
{
    using namespace boost::asio;

    auto executor = co_await this_coro::executor;
    auto endpoint = ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
    auto acceptor = ip::tcp::acceptor(executor, endpoint);
    auto socket = ip::tcp::socket(executor);
    co_await acceptor.async_accept(socket, use_awaitable);

    auto srvConfig = R"({"server_id":"NCP2P7REDTV5AAHJ3BN24CGZJ3ZEMB4WYBDGP7PRGKUNCH3PKTIYJQBD","server_name":"NCP2P7REDTV5AAHJ3BN24CGZJ3ZEMB4WYBDGP7PRGKUNCH3PKTIYJQBD","version":"2.9.17","proto":1,"git_commit":"4f2c9a5","go":"go1.19.9","host":"0.0.0.0","port":4222,"headers":true,"auth_required":true,"max_payload":1048576,"client_id":30,"client_ip":"172.17.0.1"})";
    std::string info = std::format("INFO {}\r\n", srvConfig);
    co_await async_write(socket, buffer(info), use_awaitable);

    std::string reply;
    co_await async_read_until(socket, dynamic_buffer(reply), "\r\n", use_awaitable);

    BOOST_TEST(reply.starts_with("CONNECT "));

    co_return socket;
}

auto ping_pong(ip::tcp::socket& socket) -> awaitable<void>
{
    co_await async_write(socket, buffer("PING\r\n"), use_awaitable);
    std::string buf;
    std::size_t size = co_await async_read_until(socket, dynamic_buffer(buf), "\r\n", use_awaitable);
    buf.resize(size);
    BOOST_TEST(buf == "PONG\r\n");
}

} // Anonymous namespace

BOOST_AUTO_TEST_CASE(mock)
{
    auto client = []() -> awaitable<void>
    {
        auto socket = co_await connect_to_nats("nats://token@localhost:4223");
        std::string buf;
        std::size_t size = co_await async_read_until(socket, dynamic_buffer(buf), "\r\n", use_awaitable);
        buf.resize(size);
        BOOST_TEST(buf == "PING\r\n");
        co_await async_write(socket, buffer("PONG\r\n"), use_awaitable);
    };

    auto server = []() -> awaitable<void>
    {
        auto socket = co_await mock_nats(4223);
        co_await ping_pong(socket);
    };

    auto ioContext = io_context();
    co_spawn(ioContext, client(), rethrow_handler);
    co_spawn(ioContext, server(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(reply_on_ping)
{
    Event stop;

    auto client = [&]() -> awaitable<void>
    {
        auto client = co_await createClient("nats://token@localhost:4223");
        auto result = co_await(client->run() || stop.wait(use_awaitable));
        BOOST_TEST(result.index() == 1); // stop win
    };

    auto server = [&]() -> awaitable<void>
    {
        auto socket = co_await mock_nats(4223);
        for (int i = 0; i < 3; i++) {
            co_await ping_pong(socket);
        }
        stop.set();
    };

    auto ioContext = io_context();
    co_spawn(ioContext, client(), rethrow_handler);
    co_spawn(ioContext, server(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(first_publish)
{
    auto main = [&]() -> awaitable<void>
    {
        Event stop;

        auto client = co_await createClient("nats://token@localhost:4222");
        co_spawn(
            co_await this_coro::executor,
            [&]() -> awaitable<void> { auto result = co_await(client->run() || stop.wait(use_awaitable)); BOOST_TEST(result.index() == 1); },
            rethrow_handler);

        co_await client->publish("a.b", "First publish");
        co_await async_sleep(100ms);
        stop.set();
    };

    auto ioContext = io_context();
    co_spawn(ioContext, main(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace nats_coro::test
