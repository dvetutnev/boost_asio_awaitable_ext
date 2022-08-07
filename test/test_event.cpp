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
        auto initiate = [this, executor]<typename Handler>(Handler&& handler) mutable
        {
            this->_handler = [executor, handler = std::forward<Handler>(handler)]() mutable {
                boost::asio::post(executor, std::move(handler));
            };

            State oldState = State::not_set;
            const bool isWaiting = _state.compare_exchange_strong(
                oldState,
                State::not_set_consumer_waiting,
                std::memory_order_release,
                std::memory_order_acquire);

            if (!isWaiting) {
            //    this->_handler();
            }
        };

        return boost::asio::async_initiate<
            decltype(boost::asio::use_awaitable), void()>(
            initiate, boost::asio::use_awaitable);
    }

    void set() {
        const State oldState = _state.exchange(State::set, std::memory_order_acq_rel);

        if (oldState == State::not_set_consumer_waiting) {
            _handler();
        }
    }

    enum class State { not_set, not_set_consumer_waiting, set };
    std::atomic<State> _state = State::not_set;
    std::move_only_function<void()> _handler;
};
} // Anonymous namespace

TEST(simple_event_asio, _1) {
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

TEST(simple_event_asio, _2) {
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

        event.set();
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
