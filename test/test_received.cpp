#include "received.h"
#include "utils.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/test/unit_test.hpp>

namespace nats_coro::test {

using namespace boost::asio;

BOOST_AUTO_TEST_SUITE(tests_received);

BOOST_AUTO_TEST_CASE(_)
{
    std::string msg = "MSG a.b sub1 3\r\nABC\r\n"
                      "MSG c.d sub2 7\r\nDATA417\r\n";
    //                 |            | | |
    //                 0           13   15

    auto server = [&]() -> awaitable<void>
    {
        auto executor = co_await this_coro::executor;
        auto endpoint = ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 4223);
        auto acceptor = ip::tcp::acceptor(executor, endpoint);
        auto socket = ip::tcp::socket(executor);
        co_await acceptor.async_accept(socket, use_awaitable);
        co_await async_write(socket, buffer(msg), use_awaitable);
    };

    auto client = [&]() -> awaitable<void>
    {
        auto addr = ip::make_address_v6("::1");
        auto endpoint = ip::tcp::endpoint(addr, 4223);
        auto socket = ip::tcp::socket(co_await this_coro::executor);
        co_await async_connect(socket, std::initializer_list<ip::tcp::endpoint>{endpoint}, use_awaitable);

        streambuf buf;
        decltype(buffers_begin(buf.data())) begin;
        std::size_t readed;
        std::string line;

        readed = co_await async_read_until(socket, buf, "\r\n", use_awaitable);
        BOOST_TEST(readed == 16);
        begin = buffers_begin(buf.data());
        line = std::string(begin, begin + readed);
        BOOST_TEST(line == "MSG a.b sub1 3\r\n");

        readed = co_await async_read_until(socket, buf, received(16 + 3 + 2), use_awaitable);
        BOOST_TEST(readed == 16 + 3 + 2);
        begin = buffers_begin(buf.data());
        line = std::string(begin, begin + readed);
        BOOST_TEST(line == "MSG a.b sub1 3\r\nABC\r\n");

        buf.consume(readed);

        readed = co_await async_read_until(socket, buf, "\r\n", use_awaitable);
        BOOST_TEST(readed == 16);
        begin = buffers_begin(buf.data());
        line = std::string(begin, begin + readed);
        BOOST_TEST(line == "MSG c.d sub2 7\r\n");

        readed = co_await async_read_until(socket, buf, received(16 + 7 + 2), use_awaitable);
        BOOST_TEST(readed == 16 + 7 + 2);
        begin = buffers_begin(buf.data());
        line = std::string(begin, begin + readed);
        BOOST_TEST(line == "MSG c.d sub2 7\r\nDATA417\r\n");
    };

    auto ioContext = io_context();
    co_spawn(ioContext, server(), rethrow_handler);
    co_spawn(ioContext, client(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace nats_coro::test
