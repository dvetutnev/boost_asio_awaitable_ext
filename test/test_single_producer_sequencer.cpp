#include "single_producer_sequencer.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/thread_pool.hpp>

#include <boost/test/unit_test.hpp>

namespace boost::asio::awaitable_ext::test {

BOOST_AUTO_TEST_SUITE(tests_SingleProducerSequencer);

BOOST_AUTO_TEST_CASE(claim_one)
{
    constexpr std::size_t bufferSize = 256;
    constexpr std::size_t indexMask = bufferSize - 1;
    std::uint64_t buffer[bufferSize];

    constexpr std::size_t iterationCount = 1'000'000;

    auto producer = [&](SingleProducerSequencer<std::size_t>& sequencer) -> awaitable<void>
    {
        for (std::size_t i = 1; i <= iterationCount; i++)
        {
            auto seq = co_await sequencer.claim_one();
            buffer[seq & indexMask] = i;
            sequencer.publish(seq);
        }

        auto finalSeq = co_await sequencer.claim_one();
        buffer[finalSeq & indexMask] = 0;
        sequencer.publish(finalSeq);

        co_return;
    };

    std::uint64_t result = 0;

    auto consumer = [&](const SingleProducerSequencer<std::size_t>& sequencer,
                        SequenceBarrier<std::size_t>& consumerBarrier) -> awaitable<void>
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

    SequenceBarrier<std::size_t> consumerBarrier;
    SingleProducerSequencer<std::size_t> sequencer{consumerBarrier, bufferSize};

    thread_pool tp{2};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    co_spawn(executorA, consumer(sequencer, consumerBarrier), detached);
    co_spawn(executorB, producer(sequencer), detached);
    tp.join();

    constexpr std::uint64_t expectedResult =
        static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result == expectedResult);
}

BOOST_AUTO_TEST_CASE(claim_up_to)
{
    constexpr std::size_t bufferSize = 256;
    constexpr std::size_t indexMask = bufferSize - 1;
    std::uint64_t buffer[bufferSize];

    constexpr std::size_t iterationCount = 1'000'000;

    auto producer = [&](SingleProducerSequencer<std::size_t>& sequencer) -> awaitable<void>
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

    std::uint64_t result = 0;

    auto consumer = [&](const SingleProducerSequencer<std::size_t>& sequencer,
                        SequenceBarrier<std::size_t>& consumerBarrier) -> awaitable<void>
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

    SequenceBarrier<std::size_t> consumerBarrier;
    SingleProducerSequencer<std::size_t> sequencer{consumerBarrier, bufferSize};

    thread_pool tp{2};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    co_spawn(executorA, consumer(sequencer, consumerBarrier), detached);
    co_spawn(executorB, producer(sequencer), detached);
    tp.join();

    constexpr std::uint64_t expectedResult =
        static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result == expectedResult);
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace boost::asio::awaitable_ext::test
