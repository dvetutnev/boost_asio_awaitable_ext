#include "schedule_asio.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <gtest/gtest.h>

TEST(schedule_asio, _) {
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
            EXPECT_TRUE(reachedPointA);
            EXPECT_FALSE(reachedPointB);
            co_return;
    };

    auto task = [&]() -> boost::asio::awaitable<void> {
            using namespace boost::asio::experimental::awaitable_operators;
            co_await(process() && check());
            co_return;
    };

    boost::asio::io_context ioContext;
    boost::asio::co_spawn(ioContext, task(), boost::asio::detached);
    ioContext.run();

    EXPECT_TRUE(reachedPointA);
    EXPECT_TRUE(reachedPointB);
}
