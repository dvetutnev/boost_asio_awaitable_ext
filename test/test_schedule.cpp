#include "schedule.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/test/unit_test.hpp>

namespace boost::asio::awaitable_ext::test {

BOOST_AUTO_TEST_CASE(test_schedule)
{
    bool reachedPointA = false;
    bool reachedPointB = false;

    auto process = [&]() -> awaitable<void> {
            reachedPointA = true;

            any_io_executor executor = co_await this_coro::executor;
            co_await schedule(executor);

            reachedPointB = true;
            co_return;
    };

    auto check = [&]() -> awaitable<void> {
            BOOST_TEST(reachedPointA);
            BOOST_TEST(!reachedPointB);
            co_return;
    };

    auto main = [&]() -> awaitable<void> {
            using namespace experimental::awaitable_operators;
            co_await(process() && check());
            co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, main(), detached);
    ioContext.run();

    BOOST_TEST(reachedPointA);
    BOOST_TEST(reachedPointB);
}

} // namespace boost::asio::awaitable_ext::test
