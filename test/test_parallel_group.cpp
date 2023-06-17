#include "event.h"
#include "utils.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/deferred.hpp>

#include <boost/test/unit_test.hpp>

using namespace boost::asio;
using experimental::make_parallel_group;
using experimental::wait_for_one;
using awaitable_ext::Event;

using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(tests_parallel_group);

BOOST_AUTO_TEST_CASE(throwing)
{
    Event shutdown;

    auto rx = []() -> awaitable<void>
    {
        throw boost::system::system_error{error::connection_aborted};
    };

    auto main = [&]() -> awaitable<void>
    {
        auto executor = co_await this_coro::executor;
        auto [order, rxEx, sdEx] = co_await make_parallel_group(
            co_spawn(executor, rx, deferred),
            shutdown.wait(deferred)).async_wait(
                                            wait_for_one(),
                                            deferred);
        BOOST_TEST(order[0] == 0);
        BOOST_CHECK_EXCEPTION(std::rethrow_exception(rxEx),
                              boost::system::system_error,
                              [](auto& ex){ return ex.code() == error::connection_aborted; });
    };

    auto ioContext = io_context();
    co_spawn(ioContext, main(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(cancel)
{
    Event shutdown;

    auto rx = []() -> awaitable<void>
    {
        Event dummy;
        co_await dummy.wait(use_awaitable);
    };

    auto main = [&]() -> awaitable<void>
    {
        auto executor = co_await this_coro::executor;
        auto [order, rxEx, sdEx] = co_await make_parallel_group(
                                       co_spawn(executor, rx, deferred),
                                       shutdown.wait(deferred)).async_wait(
                                           wait_for_one(),
                                           deferred);
        BOOST_TEST(order[0] == 1);
        BOOST_CHECK_EXCEPTION(std::rethrow_exception(rxEx),
                              boost::system::system_error,
                              [](auto& ex){ return ex.code() == error::operation_aborted; });
    };

    auto doShutdown = [&]() -> awaitable<void>
    {
        co_await async_sleep(1ms);
        shutdown.set();
    };

    auto ioContext = io_context();
    co_spawn(ioContext, main(), rethrow_handler);
    co_spawn(ioContext, doShutdown(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_SUITE_END();
