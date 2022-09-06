#include "event.h"
#include "schedule.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/test/unit_test.hpp>

namespace boost::asio::awaitable_ext::test {

BOOST_AUTO_TEST_CASE(test_Event)
{
    bool reachedPointA = false;
    bool reachedPointB = false;
    Event event;

    auto consumer = [&]() -> awaitable<void> {
        reachedPointA = true;

        any_io_executor executor = co_await this_coro::executor;
        co_await event.wait(executor);

        reachedPointB = true;
        co_return;
    };

    auto producer = [&]() -> awaitable<void> {
        BOOST_TEST(reachedPointA);
        BOOST_TEST(!reachedPointB);

        any_io_executor executor = co_await this_coro::executor;
        co_await schedule(executor);

        BOOST_TEST(reachedPointA);
        BOOST_TEST(!reachedPointB);

        event.set();
        co_return;
    };

    auto main = [&]() -> awaitable<void> {
        using namespace experimental::awaitable_operators;
        co_await(consumer() && producer());
        co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, main(), detached);
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

    auto consumer = [&]() -> awaitable<void> {
        reachedPointA = true;

        any_io_executor executor = co_await this_coro::executor;
        co_await event.wait(executor);

        reachedPointB = true;
        co_return;
    };

    auto producer = [&]() -> awaitable<void> {
        BOOST_TEST(reachedPointA);
        BOOST_TEST(!reachedPointB);
        co_return;
    };

    auto main = [&]() -> awaitable<void> {
        using namespace experimental::awaitable_operators;
        co_await(consumer() && producer());
        co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, main(), detached);
    ioContext.run();

    BOOST_TEST(reachedPointA);
    BOOST_TEST(reachedPointB);
}

} // namespace boost::asio::awaitable_ext::test
