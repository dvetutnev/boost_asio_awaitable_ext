#include "event.h"
#include "schedule.h"
#include "async_sleep.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/test/unit_test.hpp>

#include <chrono>

namespace boost::asio::awaitable_ext::test {

using namespace std::chrono_literals;
using namespace experimental::awaitable_operators;

BOOST_AUTO_TEST_SUITE(tests_Event);

BOOST_AUTO_TEST_CASE(simple)
{
    bool reachedPointA = false;
    bool reachedPointB = false;
    Event event;

    auto consumer = [&]() -> awaitable<void> {
        reachedPointA = true;

        co_await event.wait(use_awaitable);

        reachedPointB = true;
        co_return;
    };

    auto producer = [&]() -> awaitable<void> {
        BOOST_TEST(reachedPointA);
        BOOST_TEST(!reachedPointB);

        co_await schedule(co_await this_coro::executor);

        BOOST_TEST(reachedPointA);
        BOOST_TEST(!reachedPointB);

        event.set();
        co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, consumer(), [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    co_spawn(ioContext, producer(), [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    ioContext.run();

    BOOST_TEST(reachedPointA);
    BOOST_TEST(reachedPointB);
}

BOOST_AUTO_TEST_CASE(set_before_wait)
{
    bool reachedPointA = false;
    bool reachedPointB = false;
    Event event;

    event.set();

    auto consumer = [&]() -> awaitable<void> {
        reachedPointA = true;

        co_await event.wait(use_awaitable);

        reachedPointB = true;
        co_return;
    };

    auto producer = [&]() -> awaitable<void> {
        BOOST_TEST(reachedPointA);
        BOOST_TEST(!reachedPointB);
        co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, consumer(), [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    co_spawn(ioContext, producer(), [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    ioContext.run();

    BOOST_TEST(reachedPointA);
    BOOST_TEST(reachedPointB);
}

BOOST_AUTO_TEST_CASE(cancel)
{
    bool reachedPointA = false;
    bool reachedPointB = false;
    Event event;

    auto consumer = [&]() -> awaitable<void> {
        reachedPointA = true;
        auto [ec] = co_await event.wait(as_tuple(use_awaitable)); // -> std::tuple<boost::system::error_code>
        BOOST_TEST(ec == error::operation_aborted);
        reachedPointB = true;
    };

    auto producer = [&]() -> awaitable<void> {
        BOOST_TEST(reachedPointA);
        BOOST_TEST(!reachedPointB);
        co_await async_sleep(100ms);
        event.set();
    };

    auto timeout = [&]() -> awaitable<void> {
        co_await async_sleep(50ms);
        event.cancel();
    };

    io_context ioContext;
    co_spawn(ioContext, consumer(), [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    co_spawn(ioContext, producer(), [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    co_spawn(ioContext, timeout(), [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    ioContext.run();

    BOOST_TEST(reachedPointA);
    BOOST_TEST(reachedPointB);
}

BOOST_AUTO_TEST_CASE(cancel_before_wait)
{
    bool reachedPointA = false;
    bool reachedPointB = false;
    Event event;

    event.cancel();

    auto consumer = [&]() -> awaitable<void> {
        reachedPointA = true;
        auto [ec] = co_await event.wait(as_tuple(use_awaitable)); // -> std::tuple<boost::system::error_code>
        BOOST_TEST(ec == error::operation_aborted);
        reachedPointB = true;
    };

    auto producer = [&]() -> awaitable<void> {
        BOOST_TEST(reachedPointA);
        BOOST_TEST(!reachedPointB);
        co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, consumer(), [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    co_spawn(ioContext, producer(), [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    ioContext.run();

    BOOST_TEST(reachedPointA);
    BOOST_TEST(reachedPointB);
}

BOOST_AUTO_TEST_CASE(install_cancellation_slot)
{
    Event event;

    auto main = [&]() -> awaitable<void> {
        auto result = co_await(
            event.wait(use_awaitable) ||
            async_sleep(50ms)
            );
        BOOST_TEST(result.index() == 1); // timer first
    };

    io_context ioContext;
    co_spawn(ioContext, main(), [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(cancel_example)
{
    Event event;

    auto consumer = [&]() -> awaitable<void> {
        auto [ec] = co_await event.wait(as_tuple(use_awaitable)); // -> std::tuple<boost::system::error_code>
        BOOST_TEST(ec == error::operation_aborted);
    };

    auto timeout = [&]() -> awaitable<void> {
        co_await async_sleep(50ms);
        event.cancel();
    };

    io_context ioContext;
    co_spawn(ioContext, consumer(), [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    co_spawn(ioContext, timeout(), [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    ioContext.run();
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace boost::asio::awaitable_ext::test
