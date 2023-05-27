#include "sequence_barrier_group.h"
#include "single_producer_sequencer.h"
#include "schedule.h"
#include "utils.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>

#include <boost/test/unit_test.hpp>

namespace boost::asio::awaitable_ext::test {

using namespace experimental::awaitable_operators;
using namespace std::chrono_literals;

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
        co_await(consumer() && producer());
        co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, main(), detached);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(parallel)
{
    constexpr std::size_t bufferSize = 256;
    constexpr std::size_t indexMask = bufferSize - 1;
    std::uint64_t buffer[bufferSize];

    constexpr std::size_t iterationCount = 1'000'000;

    using Sequencer = SingleProducerSequencer<std::size_t, SequenceTraits<std::size_t>, SequenceBarrierGroup<std::size_t>>;

    auto producer = [&](Sequencer& sequencer) -> awaitable<void>
    {
        constexpr std::size_t maxBatchSize = 10;

        std::size_t i = 0;
        while (i < iterationCount)
        {
            std::size_t batchSize = std::min(maxBatchSize, iterationCount - i);
            auto range = co_await sequencer.claim_up_to(batchSize);
            for (auto seq : range) {
                buffer[seq & indexMask] = ++i;
            }
            sequencer.publish(range);
        }

        auto finalSeq = co_await sequencer.claim_one();
        buffer[finalSeq & indexMask] = 0;
        sequencer.publish(finalSeq);

        co_return;
    };

    auto consumer = [&](const Sequencer& sequencer,
                        SequenceBarrier<std::size_t>& consumerBarrier,
                        std::size_t& result) -> awaitable<void>
    {
        bool reachedEnd = false;
        std::size_t nextToRead = 0;
        do {
            const std::size_t available = co_await sequencer.wait_until_published(nextToRead);
            do {
                result += buffer[nextToRead & indexMask];
            } while (nextToRead++ != available);

            // Zero value is sentinel that indicates the end of the stream.
            reachedEnd = buffer[available & indexMask] == 0;

            // Notify that we've finished processing up to 'available'.
            consumerBarrier.publish(available);

        } while (!reachedEnd);

        co_return;
    };

    SequenceBarrier<std::size_t> barrier1, barrier2;
    SequenceBarrierGroup<std::size_t> barrierGroup{{barrier1, barrier2}};
    Sequencer sequencer{barrierGroup, bufferSize};

    std::size_t result1 = 0;
    std::size_t result2 = 0;

    thread_pool tp{3};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    any_io_executor executorC = tp.get_executor();
    co_spawn(executorA, consumer(sequencer, barrier1, result1), detached);
    co_spawn(executorB, consumer(sequencer, barrier2, result2), detached);
    co_spawn(executorC, producer(sequencer), detached);
    tp.join();

    constexpr std::uint64_t expectedResult =
        static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result1 == expectedResult);
    BOOST_TEST(result2 == expectedResult);
}

BOOST_AUTO_TEST_CASE(unique)
{
    constexpr std::size_t bufferSize = 256;
    constexpr std::size_t indexMask = bufferSize - 1;
    std::uint64_t buffer[bufferSize];

    constexpr std::size_t iterationCount = 1'000'000;

    using Traits = SequenceTraits<std::size_t>;
    using Sequencer = SingleProducerSequencer<std::size_t, Traits, SequenceBarrierGroup<std::size_t>>;

    auto producer = [&](Sequencer& sequencer, unsigned consumerCount) -> awaitable<void>
    {
        constexpr std::size_t maxBatchSize = 10;

        std::size_t i = 0;
        while (i < iterationCount)
        {
            std::size_t batchSize = std::min(maxBatchSize, iterationCount - i);
            auto range = co_await sequencer.claim_up_to(batchSize);
            for (auto seq : range) {
                buffer[seq & indexMask] = ++i;
            }
            sequencer.publish(range);
        }

        while (consumerCount--)
        {
            auto finalSeq = co_await sequencer.claim_one();
            buffer[finalSeq & indexMask] = 0;
            sequencer.publish(finalSeq);
        };

        co_return;
    };

    auto consumer = [&](const Sequencer& sequencer,
                        SequenceBarrier<std::size_t>& readBarrier,
                        std::atomic_size_t& nextToRead,
                        std::size_t& result) -> awaitable<void>
    {
        bool reachedEnd = false;
        std::size_t lastKnownPublished = readBarrier.last_published();
        do
        {
            std::size_t toRead = nextToRead.fetch_add(1);
            if (Traits::difference(toRead, lastKnownPublished) >= bufferSize)
            {
                // Move consumer barrier if retarded
                readBarrier.publish(toRead - 1);
            }

            co_await sequencer.wait_until_published(toRead);
            result += buffer[toRead & indexMask];

            // Zero value is sentinel that indicates the end of the stream.
            reachedEnd = buffer[toRead & indexMask] == 0;

            // Notify that we've finished processing up to 'available'.
            readBarrier.publish(toRead);
            lastKnownPublished = toRead;
        } while (!reachedEnd);

        co_return;
    };

    SequenceBarrier<std::size_t> barrier1, barrier2;
    SequenceBarrierGroup<std::size_t> barrierGroup{{barrier1, barrier2}};
    Sequencer sequencer{barrierGroup, bufferSize};

    std::atomic_size_t nextToRead{0};

    std::size_t result1 = 0;
    std::size_t result2 = 0;

    thread_pool tp{3};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    any_io_executor executorC = tp.get_executor();
    co_spawn(executorA, consumer(sequencer, barrier1, nextToRead, result1), detached);
    co_spawn(executorB, consumer(sequencer, barrier2, nextToRead, result2), detached);
    co_spawn(executorC, producer(sequencer, 2), detached);
    tp.join();

    constexpr std::uint64_t expectedResult =
        static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result1 + result2 == expectedResult);
}

BOOST_AUTO_TEST_CASE(cancellation)
{
    SequenceBarrier<std::size_t> barrier1, barrier2;
    SequenceBarrierGroup<std::size_t> barrierGroup{{barrier1, barrier2}};

    auto consumer1 = [&]() -> awaitable<void>
    {
        auto result = co_await(
            barrierGroup.wait_until_published(0) ||
            async_sleep(10ms)
            );
        BOOST_TEST(result.index() == 1); // timer win
    };

    bool consumer2Canceled = false;
    auto consumer2 = [&]() -> awaitable<void>
    {
        try {
            co_await barrier2.wait_until_published(0);
            BOOST_FAIL("Exception not throwed");
        } catch (const system::system_error& ex) {
            consumer2Canceled = true;
            BOOST_TEST(ex.code() == error::operation_aborted);
        }
    };

    io_context ioContext;
    co_spawn(ioContext, consumer1, rethrow_handler);
    co_spawn(ioContext, consumer2, rethrow_handler);
    ioContext.run();

    BOOST_TEST(consumer2Canceled);
}

BOOST_AUTO_TEST_CASE(cancellation_from_barrier)
{
    SequenceBarrier<std::size_t> barrier1, barrier2;
    SequenceBarrierGroup<std::size_t> barrierGroup{{barrier1, barrier2}};

    auto consumer = [&]() -> awaitable<void>
    {
        try {
            co_await barrierGroup.wait_until_published(0);
            BOOST_FAIL("Exception not throwed");
        } catch (const system::system_error& ex) {
            BOOST_TEST(ex.code() == error::operation_aborted);
        }
    };

    auto closeBarrier = [&]() -> awaitable<void> { barrier2.close(); co_return; };

    io_context ioContext;
    co_spawn(ioContext, consumer(), rethrow_handler);
    co_spawn(ioContext, closeBarrier(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(close)
{
    SequenceBarrier<std::size_t> barrier1, barrier2;
    SequenceBarrierGroup<std::size_t> barrierGroup{{barrier1, barrier2}};
    barrierGroup.close();
    BOOST_TEST(barrier1.is_closed());
    BOOST_TEST(barrier2.is_closed());
}

BOOST_AUTO_TEST_CASE(is_closing)
{
    SequenceBarrier<std::size_t> barrier1, barrier2, barrier3;
    SequenceBarrierGroup<std::size_t> barrierGroup{{barrier1, barrier2, barrier3}};
    barrier2.close();
    BOOST_TEST(barrierGroup.is_closed());
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace boost::asio::awaitable_ext::test
