#include "schedule.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(test_schedule)
{
    bool reachedPointA = false;
    bool reachedPointB = false;

    auto process = [&]() -> boost::asio::awaitable<void> {
            reachedPointA = true;

            boost::asio::any_io_executor executor = co_await boost::asio::this_coro::executor;
            co_await schedule(executor);

            reachedPointB = true;
            co_return;
    };

    auto check = [&]() -> boost::asio::awaitable<void> {
            BOOST_TEST(reachedPointA);
            BOOST_TEST(!reachedPointB);
            co_return;
    };

    auto app = [&]() -> boost::asio::awaitable<void> {
            using namespace boost::asio::experimental::awaitable_operators;
            co_await(process() && check());
            co_return;
    };

    boost::asio::io_context ioContext;
    boost::asio::co_spawn(ioContext, app(), boost::asio::detached);
    ioContext.run();

    BOOST_TEST(reachedPointA);
    BOOST_TEST(reachedPointB);
}
