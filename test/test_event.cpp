#include "schedule_asio.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <gtest/gtest.h>

namespace {
struct Event
{
    boost::asio::awaitable<void> wait(boost::asio::any_io_executor executor) {
        auto initiate = [executor]<typename Handler>(Handler&& handler) mutable
        {
            boost::asio::post(executor, [handler = std::forward<Handler>(handler)]() mutable
                              {
                                  handler();
                              });
        };

        return boost::asio::async_initiate<
            decltype(boost::asio::use_awaitable), void()>(
            initiate, boost::asio::use_awaitable);
    }

    void resume() {}
};
} // Anonymous namespace

TEST(simple_event_asio, _) {
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
