#include "sequence_barrier_group.h"
#include "schedule.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>

#include <boost/test/unit_test.hpp>

namespace boost::asio::awaitable_ext::test {

BOOST_AUTO_TEST_SUITE(tests_SequenceBarrierGroup);

BOOST_AUTO_TEST_CASE(_)
{
    SequenceBarrier<std::size_t> barrier1;
    SequenceBarrier<std::size_t> barrier2;
    SequenceBarrierGroup<std::size_t> group{ {barrier1, barrier2} };

    bool reachedA = false;
    bool reachedB = false;
    bool reachedC = false;

    auto consumer = [&]() -> awaitable<void>
    {
        co_await group.wait_until_published(0);
        reachedA = true;

        co_await group.wait_until_published(10);
        reachedB = true;

        co_await group.wait_until_published(20);
        reachedC = true;

        co_return;
    };

    auto producer = [&]() -> awaitable<void>
    {
        BOOST_TEST(!reachedA);
        BOOST_TEST(!reachedB);

        barrier1.publish(0);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(!reachedA);

        barrier2.publish(0);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(reachedA);

        BOOST_TEST(!reachedB);

        barrier1.publish(11);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(!reachedB);

        barrier2.publish(9);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(!reachedB);

        barrier2.publish(10);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(reachedB);

        BOOST_TEST(!reachedC);

        barrier2.publish(22);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(!reachedC);

        barrier1.publish(18);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(!reachedC);

        barrier1.publish(21);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(reachedC);

        co_return;
    };

    auto main = [&]() -> awaitable<void>
    {
        using namespace experimental::awaitable_operators;
        co_await(consumer() && producer());
        co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, main(), detached);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(return_earliest)
{
    SequenceBarrier<std::uint8_t> barrier1;
    SequenceBarrier<std::uint8_t> barrier2;
    SequenceBarrierGroup<std::uint8_t> group{ {barrier1, barrier2} };

    auto consumer = [&]() -> awaitable<void>
    {
        BOOST_TEST(co_await group.wait_until_published(0) == 0);
        BOOST_TEST(co_await group.wait_until_published(10) == 11);
        BOOST_TEST(co_await group.wait_until_published(20) == 21);

        co_return;
    };

    auto producer = [&]() -> awaitable<void>
    {
        barrier1.publish(0);
        barrier2.publish(1);
        co_await schedule(co_await this_coro::executor);

        barrier1.publish(12);
        barrier2.publish(11);
        co_await schedule(co_await this_coro::executor);

        barrier1.publish(21);
        barrier2.publish(22);
        co_await schedule(co_await this_coro::executor);

        co_return;
    };

    auto main = [&]() -> awaitable<void>
    {
        using namespace experimental::awaitable_operators;
        co_await(consumer() && producer());
        co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, main(), detached);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(return_earliest2)
{
    constexpr std::uint8_t initialSequence = 240u;

    SequenceBarrier<std::uint8_t> barrier1{initialSequence};
    SequenceBarrier<std::uint8_t> barrier2{initialSequence};
    SequenceBarrierGroup<std::uint8_t> group{ {barrier1, barrier2} };

    auto consumer = [&]() -> awaitable<void>
    {
        BOOST_TEST(co_await group.wait_until_published(250u) == 253u);

        co_return;
    };

    auto producer = [&]() -> awaitable<void>
    {
        barrier1.publish(7);
        barrier2.publish(253u);

        co_return;
    };

    auto main = [&]() -> awaitable<void>
    {
        using namespace experimental::awaitable_operators;
        co_await(consumer() && producer());
        co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, main(), detached);
    ioContext.run();
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace boost::asio::awaitable_ext::test
