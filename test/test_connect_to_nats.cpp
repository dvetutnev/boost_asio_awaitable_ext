#include "connect_to_nats.h"
#include "utils.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/test/unit_test.hpp>

namespace nats_coro::test {

BOOST_AUTO_TEST_SUITE(nats_coro);

BOOST_AUTO_TEST_CASE(_)
{
    auto main = [](std::string_view host,
                   std::string_view port,
                   std::string_view token) -> awaitable<void>
    {
        ip::tcp::socket socket = co_await connect_to_nats(host, port, token);
        co_await async_write(socket, buffer("PUB a.b 2\r\n79\r\n"), use_awaitable);

        std::string buf;
        co_await async_read_until(socket, dynamic_buffer(buf), "\r\n", use_awaitable);
        std::cout << buf << std::endl;
    };

    auto ioContext = io_context();
    co_spawn(ioContext, main("localhost", "4222", "token"), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace nats_coro::test
