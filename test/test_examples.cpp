#include "async_sleep.h"

#include <boost/asio/system_timer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/as_tuple.hpp>

#include <boost/test/unit_test.hpp>

namespace boost::asio::awaitable_ext::test {

using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(tests_Examples);

BOOST_AUTO_TEST_CASE(timer)
{
    io_context ioContext;

    auto start = std::chrono::system_clock::now();

    co_spawn(ioContext, async_sleep(150ms), detached);
    ioContext.run();

    auto duration = std::chrono::system_clock::now() - start;

    BOOST_TEST(duration >= 100ms);
    BOOST_TEST(duration <= 200ms);
}

BOOST_AUTO_TEST_CASE(cancel_timer_when_destroy)
{
    auto main = []() -> awaitable<void>
    {
        using namespace experimental::awaitable_operators;

        auto start = std::chrono::system_clock::now();

        auto result = co_await(
            async_sleep(100ms) ||
            async_sleep(200ms)
            );

        auto duration = std::chrono::system_clock::now() - start;

        BOOST_TEST(result.index() == 0);
        BOOST_TEST(duration >= 100ms);
        BOOST_TEST(duration < 150ms);
    };

    io_context ioContext;
    co_spawn(ioContext, main(), [](std::exception_ptr ex) { if (ex) std::rethrow_exception(ex); });
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(first_timer)
{
    io_context ioContext;
    system_timer timerA{ioContext}, timerB{ioContext};
    timerA.expires_after(100ms); timerB.expires_after(200ms);

    auto main = [&]() -> awaitable<void>
    {
        using namespace experimental::awaitable_operators;

        auto start = std::chrono::system_clock::now();

        auto result = co_await(
            timerA.async_wait(use_awaitable) ||
            timerB.async_wait(use_awaitable)
            );

        auto duration = std::chrono::system_clock::now() - start;

        BOOST_TEST(result.index() == 0);
        BOOST_TEST(duration >= 100ms);
        BOOST_TEST(duration < 150ms);
    };

    co_spawn(ioContext, main(), [](std::exception_ptr ex) { if (ex) std::rethrow_exception(ex); });
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(explicit_cancel_timer)
{
    io_context ioContext;
    system_timer cancelableTimer{ioContext};

    auto waitCancelableTimer = [&]() -> awaitable<void>
    {
        constexpr auto use_nothrow_awaitable = experimental::as_tuple(use_awaitable);

        cancelableTimer.expires_after(200ms);
        auto start = std::chrono::system_clock::now();
        auto [ ec ] = co_await cancelableTimer.async_wait(use_nothrow_awaitable); // -> std::tuple<boost::system::error_code>
        BOOST_TEST(ec == error::operation_aborted);
        auto duration = std::chrono::system_clock::now() - start;

        BOOST_TEST(duration >= 100ms);
        BOOST_TEST(duration < 150ms);
    };

    auto cancelTimerAfter = [&](std::chrono::milliseconds duration) -> awaitable<void>
    {
        co_await async_sleep(duration);
        cancelableTimer.cancel();
    };

    co_spawn(ioContext, waitCancelableTimer(), [](std::exception_ptr ex) { if (ex) std::rethrow_exception(ex); });
    co_spawn(ioContext, cancelTimerAfter(100ms), [](std::exception_ptr ex) { if (ex) std::rethrow_exception(ex); });
    ioContext.run();
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace awaitable_ext::test
