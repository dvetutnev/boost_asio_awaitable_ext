#include "event.h"
#include "schedule.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <gtest/gtest.h>

namespace {
} // Anonymous namespace

TEST(Event, _) {
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

    auto check = [&]() -> boost::asio::awaitable<void> {
        EXPECT_TRUE(reachedPointA);
        EXPECT_FALSE(reachedPointB);

        boost::asio::any_io_executor executor = co_await boost::asio::this_coro::executor;
        co_await schedule(executor);

        EXPECT_TRUE(reachedPointA);
        EXPECT_FALSE(reachedPointB);

        event.set();
        co_return;
    };

    auto task = [&]() -> boost::asio::awaitable<void> {
        using namespace boost::asio::experimental::awaitable_operators;
        co_await(consumer() && check());
        co_return;
    };

    boost::asio::io_context ioContext;
    boost::asio::co_spawn(ioContext, task(), boost::asio::detached);
    ioContext.run();

    EXPECT_TRUE(reachedPointA);
    EXPECT_TRUE(reachedPointB);
}

TEST(Event, set_before_wait) {
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

    auto check = [&]() -> boost::asio::awaitable<void> {
        EXPECT_TRUE(reachedPointA);
        EXPECT_FALSE(reachedPointB);
        co_return;
    };

    auto task = [&]() -> boost::asio::awaitable<void> {
        using namespace boost::asio::experimental::awaitable_operators;
        co_await(consumer() && check());
        co_return;
    };

    boost::asio::io_context ioContext;
    boost::asio::co_spawn(ioContext, task(), boost::asio::detached);
    ioContext.run();

    EXPECT_TRUE(reachedPointA);
    EXPECT_TRUE(reachedPointB);
}
