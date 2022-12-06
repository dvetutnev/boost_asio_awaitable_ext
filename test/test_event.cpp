#include "event.h"
#include "schedule.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/thread_pool.hpp>

#include <boost/test/unit_test.hpp>

#include <thread>

namespace boost::asio::awaitable_ext::test {

BOOST_AUTO_TEST_SUITE(tests_Event);
BOOST_AUTO_TEST_CASE(simple)
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

BOOST_AUTO_TEST_CASE(set_before_wait)
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

namespace {
void multithread_test_func(std::size_t count) {
    thread_pool tp{2};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();

    for (std::size_t i = 0; i < count; i++)
    {
        Event event;
        std::atomic_bool consumerDone{false};
        std::atomic_bool producerDone{false};

        auto consumer = [&]() -> awaitable<void> {
            co_await event.wait(co_await this_coro::executor);
            consumerDone.store(true);
            co_return;
        };

        auto producer = [&]() -> awaitable<void> {
            event.set();
            producerDone.store(true);
            co_return;
        };

        co_spawn((i % 2) ? executorA : executorB, consumer(), detached);
        co_spawn((i % 2) ? executorB : executorA, producer(), detached);

        while (!consumerDone.load() || !producerDone.load())
            ;
    };

    tp.join();
}
} // Anonymous namespace

BOOST_AUTO_TEST_CASE(multithread_1) { multithread_test_func(1); }
BOOST_AUTO_TEST_CASE(multithread_10k) { multithread_test_func(10'000); }

BOOST_AUTO_TEST_SUITE_END();

} // namespace boost::asio::awaitable_ext::test
