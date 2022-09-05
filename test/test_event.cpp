#include "event.h"
#include "schedule.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(test_Event)
{
    bool reachedPointA = false;
    bool reachedPointB = false;
    Event event;

    auto consumer = [&]() -> boost::asio::awaitable<void> {
        reachedPointA = true;

        boost::asio::any_io_executor executor = co_await boost::asio::this_coro::executor;
        co_await event.wait(executor);

        reachedPointB = true;
        co_return;
    };

    auto producer = [&]() -> boost::asio::awaitable<void> {
        BOOST_TEST(reachedPointA);
        BOOST_TEST(!reachedPointB);

        boost::asio::any_io_executor executor = co_await boost::asio::this_coro::executor;
        co_await schedule(executor);

        BOOST_TEST(reachedPointA);
        BOOST_TEST(!reachedPointB);

        event.set();
        co_return;
    };

    auto main = [&]() -> boost::asio::awaitable<void> {
        using namespace boost::asio::experimental::awaitable_operators;
        co_await(consumer() && producer());
        co_return;
    };

    boost::asio::io_context ioContext;
    boost::asio::co_spawn(ioContext, main(), boost::asio::detached);
    ioContext.run();

    BOOST_TEST(reachedPointA);
    BOOST_TEST(reachedPointB);
}

BOOST_AUTO_TEST_CASE(test_Event_set_before_wait)
{
    bool reachedPointA = false;
    bool reachedPointB = false;
    Event event;

    event.set();

    auto consumer = [&]() -> boost::asio::awaitable<void> {
        reachedPointA = true;

        boost::asio::any_io_executor executor = co_await boost::asio::this_coro::executor;
        co_await event.wait(executor);

        reachedPointB = true;
        co_return;
    };

    auto producer = [&]() -> boost::asio::awaitable<void> {
        BOOST_TEST(reachedPointA);
        BOOST_TEST(!reachedPointB);
        co_return;
    };

    auto main = [&]() -> boost::asio::awaitable<void> {
        using namespace boost::asio::experimental::awaitable_operators;
        co_await(consumer() && producer());
        co_return;
    };

    boost::asio::io_context ioContext;
    boost::asio::co_spawn(ioContext, main(), boost::asio::detached);
    ioContext.run();

    BOOST_TEST(reachedPointA);
    BOOST_TEST(reachedPointB);
}
