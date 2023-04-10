#include "multi_producer_sequencer.h"
#include "sequence_barrier_group.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/thread_pool.hpp>

#include <boost/test/unit_test.hpp>

namespace boost::asio::awaitable_ext::test {

BOOST_AUTO_TEST_SUITE(tests_MultiProducerSequencer)

BOOST_AUTO_TEST_SUITE(last_published_after)

constexpr std::size_t bufferSize = 16;

BOOST_AUTO_TEST_CASE(initial)
{
    SequenceBarrier<std::uint8_t> consumerBarrier;
    MultiProducerSequencer<std::uint8_t> sequencer{consumerBarrier, bufferSize};

    BOOST_TEST(sequencer.last_published_after(0u) == 0u);
    BOOST_TEST(sequencer.last_published_after(1u) == 1u);
    BOOST_TEST(sequencer.last_published_after(2u) == 2u);
    BOOST_TEST(sequencer.last_published_after(3u) == 3u);
    BOOST_TEST(sequencer.last_published_after(4u) == 4u);
    BOOST_TEST(sequencer.last_published_after(5u) == 5u);
    BOOST_TEST(sequencer.last_published_after(6u) == 6u);
    BOOST_TEST(sequencer.last_published_after(7u) == 7u);
    BOOST_TEST(sequencer.last_published_after(8u) == 8u);
    BOOST_TEST(sequencer.last_published_after(9u) == 9u);
    BOOST_TEST(sequencer.last_published_after(10u) == 10u);
    BOOST_TEST(sequencer.last_published_after(11u) == 11u);
    BOOST_TEST(sequencer.last_published_after(12u) == 12u);
    BOOST_TEST(sequencer.last_published_after(13u) == 13u);
    BOOST_TEST(sequencer.last_published_after(14u) == 14u);
    BOOST_TEST(sequencer.last_published_after(15u) == 15u);
    BOOST_TEST(sequencer.last_published_after(16u) == 16u);
    BOOST_TEST(sequencer.last_published_after(17u) == 17u);
    BOOST_TEST(sequencer.last_published_after(18u) == 18u);
    BOOST_TEST(sequencer.last_published_after(19u) == 19u);
    BOOST_TEST(sequencer.last_published_after(20u) == 20u);
    BOOST_TEST(sequencer.last_published_after(21u) == 21u);

    BOOST_TEST(sequencer.last_published_after(236u) == 236u);
    BOOST_TEST(sequencer.last_published_after(237u) == 237u);
    BOOST_TEST(sequencer.last_published_after(238u) == 238u);
    BOOST_TEST(sequencer.last_published_after(239u) == 255u);
    BOOST_TEST(sequencer.last_published_after(240u) == 255u);
    BOOST_TEST(sequencer.last_published_after(241u) == 255u);
    BOOST_TEST(sequencer.last_published_after(242u) == 255u);
    BOOST_TEST(sequencer.last_published_after(243u) == 255u);
    BOOST_TEST(sequencer.last_published_after(244u) == 255u);
    BOOST_TEST(sequencer.last_published_after(245u) == 255u);
    BOOST_TEST(sequencer.last_published_after(246u) == 255u);
    BOOST_TEST(sequencer.last_published_after(247u) == 255u);
    BOOST_TEST(sequencer.last_published_after(248u) == 255u);
    BOOST_TEST(sequencer.last_published_after(249u) == 255u);
    BOOST_TEST(sequencer.last_published_after(250u) == 255u);
    BOOST_TEST(sequencer.last_published_after(251u) == 255u);
    BOOST_TEST(sequencer.last_published_after(252u) == 255u);
    BOOST_TEST(sequencer.last_published_after(253u) == 255u);
    BOOST_TEST(sequencer.last_published_after(254u) == 255u);
    BOOST_TEST(sequencer.last_published_after(255u) == 255u);
}

BOOST_AUTO_TEST_CASE(_3_0)
{
    SequenceBarrier<std::uint8_t> consumerBarrier;
    MultiProducerSequencer<std::uint8_t> sequencer{consumerBarrier, bufferSize};

    sequencer.publish(3);
    BOOST_TEST(sequencer.last_published_after(255u) == 255u);
    BOOST_TEST(sequencer.last_published_after(0u) == 0u);
    BOOST_TEST(sequencer.last_published_after(1u) == 1u);
    BOOST_TEST(sequencer.last_published_after(2u) == 3u);
    BOOST_TEST(sequencer.last_published_after(3u) == 3u);
    BOOST_TEST(sequencer.last_published_after(4u) == 4u);
    BOOST_TEST(sequencer.last_published_after(5u) == 5u);

    BOOST_TEST(sequencer.last_published_after(15u) == 15u);
    BOOST_TEST(sequencer.last_published_after(16u) == 16u);
    BOOST_TEST(sequencer.last_published_after(17u) == 17u);
    BOOST_TEST(sequencer.last_published_after(18u) == 18u);
    BOOST_TEST(sequencer.last_published_after(19u) == 19u);

    sequencer.publish(0);
    BOOST_TEST(sequencer.last_published_after(255u) == 0);
    BOOST_TEST(sequencer.last_published_after(0u) == 0u);
    BOOST_TEST(sequencer.last_published_after(1u) == 1u);
    BOOST_TEST(sequencer.last_published_after(2u) == 3u);
    BOOST_TEST(sequencer.last_published_after(3u) == 3u);
    BOOST_TEST(sequencer.last_published_after(4u) == 4u);
    BOOST_TEST(sequencer.last_published_after(5u) == 5u);

    BOOST_TEST(sequencer.last_published_after(15u) == 15u);
    BOOST_TEST(sequencer.last_published_after(16u) == 16u);
    BOOST_TEST(sequencer.last_published_after(17u) == 17u);
    BOOST_TEST(sequencer.last_published_after(18u) == 18u);
    BOOST_TEST(sequencer.last_published_after(19u) == 19u);
}

BOOST_AUTO_TEST_CASE(_0_3)
{
    SequenceBarrier<std::uint8_t> consumerBarrier;
    MultiProducerSequencer<std::uint8_t> sequencer{consumerBarrier, bufferSize};

    sequencer.publish(0);
    BOOST_TEST(sequencer.last_published_after(255u) == 0);
    BOOST_TEST(sequencer.last_published_after(0u) == 0u);
    BOOST_TEST(sequencer.last_published_after(1u) == 1u);
    BOOST_TEST(sequencer.last_published_after(2u) == 2u);
    BOOST_TEST(sequencer.last_published_after(3u) == 3u);
    BOOST_TEST(sequencer.last_published_after(4u) == 4u);
    BOOST_TEST(sequencer.last_published_after(5u) == 5u);

    BOOST_TEST(sequencer.last_published_after(15u) == 15u);
    BOOST_TEST(sequencer.last_published_after(16u) == 16u);
    BOOST_TEST(sequencer.last_published_after(17u) == 17u);
    BOOST_TEST(sequencer.last_published_after(18u) == 18u);
    BOOST_TEST(sequencer.last_published_after(19u) == 19u);

    sequencer.publish(3);
    BOOST_TEST(sequencer.last_published_after(255u) == 0u);
    BOOST_TEST(sequencer.last_published_after(0u) == 0u);
    BOOST_TEST(sequencer.last_published_after(1u) == 1u);
    BOOST_TEST(sequencer.last_published_after(2u) == 3u);
    BOOST_TEST(sequencer.last_published_after(3u) == 3u);
    BOOST_TEST(sequencer.last_published_after(4u) == 4u);
    BOOST_TEST(sequencer.last_published_after(5u) == 5u);

    BOOST_TEST(sequencer.last_published_after(15u) == 15u);
    BOOST_TEST(sequencer.last_published_after(16u) == 16u);
    BOOST_TEST(sequencer.last_published_after(17u) == 17u);
    BOOST_TEST(sequencer.last_published_after(18u) == 18u);
    BOOST_TEST(sequencer.last_published_after(19u) == 19u);
}

BOOST_AUTO_TEST_CASE(serial)
{
    SequenceBarrier<std::uint8_t> consumerBarrier;
    MultiProducerSequencer<std::uint8_t> sequencer{consumerBarrier, bufferSize};

    sequencer.publish(2);
    sequencer.publish(3);
    sequencer.publish(4);
    BOOST_TEST(sequencer.last_published_after(255u) == 255u);
    BOOST_TEST(sequencer.last_published_after(0u) == 0u);
    BOOST_TEST(sequencer.last_published_after(1u) == 4u);
    BOOST_TEST(sequencer.last_published_after(2u) == 4u);
    BOOST_TEST(sequencer.last_published_after(3u) == 4u);
    BOOST_TEST(sequencer.last_published_after(4u) == 4u);
    BOOST_TEST(sequencer.last_published_after(5u) == 5u);
}

BOOST_AUTO_TEST_SUITE_END() // last_published_after

namespace {

template<typename Sequencer>
awaitable<void> one_at_a_time_producer(Sequencer& sequencer,
                                       std::uint64_t buffer[],
                                       std::uint64_t iterationCount) {
    assert(iterationCount > 0);
    const std::size_t indexMask = sequencer.buffer_size() - 1;

    std::uint64_t i = 0;
    while (i < iterationCount)
    {
        auto seq = co_await sequencer.claim_one();
        buffer[seq & indexMask] = ++i;
        sequencer.publish(seq);
    }

    auto finalSeq = co_await sequencer.claim_one();
    buffer[finalSeq & indexMask] = 0;
    sequencer.publish(finalSeq);
    co_return;
}

template<typename Sequencer>
awaitable<void> batch_producer(Sequencer& sequencer,
                               std::uint64_t buffer[],
                               std::uint64_t iterationCount,
                               std::size_t maxBatchSize)
{
    assert(iterationCount > 0);
    assert(maxBatchSize > 0);
    const std::size_t indexMask = sequencer.buffer_size() - 1;

    std::uint64_t i = 0;
    while (i < iterationCount)
    {
        const std::size_t batchSize = static_cast<std::size_t>(
            std::min<std::uint64_t>(maxBatchSize, iterationCount - i));
        auto sequences = co_await sequencer.claim_up_to(batchSize);
        for (auto seq : sequences)
        {
            buffer[seq & indexMask] = ++i;
        }
        sequencer.publish(sequences);
    }

    auto finalSeq = co_await sequencer.claim_one();
    buffer[finalSeq & indexMask] = 0;
    sequencer.publish(finalSeq);
}

template<typename Sequencer>
awaitable<std::uint64_t> consumer(const Sequencer& sequencer,
                                  SequenceBarrier<std::size_t>& readBarrier,
                                  const std::uint64_t buffer[],
                                  unsigned producerCount) {
    assert(producerCount > 0);
    const std::size_t indexMask = sequencer.buffer_size() - 1;

    std::uint64_t sum = 0;

    unsigned endCount = 0;
    std::size_t nextToRead = 0;

    do
    {
        std::size_t available = co_await sequencer.wait_until_published(nextToRead, nextToRead - 1);
        do
        {
            const auto& value = buffer[nextToRead & indexMask];
            sum += value;

            // Zero value is sentinel that indicates the end of one of the streams.
            const bool isEndOfStream = value == 0;
            endCount += isEndOfStream ? 1 : 0;
        } while (nextToRead++ != available);

        // Notify that we've finished processing up to 'available'.
        readBarrier.publish(available);
    } while (endCount < producerCount);

    co_return sum;
}

} // Anonymous namespace

BOOST_AUTO_TEST_SUITE(single_consumer)

BOOST_AUTO_TEST_CASE(two_producer_single)
{
    constexpr std::size_t bufferSize = 16384;
    std::uint64_t buffer[bufferSize];

    constexpr std::size_t iterationCount = 1'000'000;
    constexpr unsigned producerCount = 2;

    SequenceBarrier<std::size_t> readBarrier;
    MultiProducerSequencer<std::size_t> sequencer{readBarrier, bufferSize};

    thread_pool tp{3};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    any_io_executor executorC = tp.get_executor();
    co_spawn(executorA, one_at_a_time_producer(sequencer, buffer, iterationCount), detached);
    co_spawn(executorB, one_at_a_time_producer(sequencer, buffer, iterationCount), detached);

    std::uint64_t result;
    co_spawn(executorC, consumer(sequencer, readBarrier, buffer, producerCount),
             [&](std::exception_ptr, std::uint64_t val) { result = val; } );
    tp.join();

    constexpr std::uint64_t expectedResult =
        producerCount * static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result == expectedResult);
}

BOOST_AUTO_TEST_CASE(two_producer_batch)
{
    constexpr std::size_t bufferSize = 16384;
    std::uint64_t buffer[bufferSize];

    constexpr std::size_t iterationCount = 1'000'000;
    constexpr unsigned producerCount = 2;
    constexpr std::size_t batchSize = 10;

    SequenceBarrier<std::size_t> readBarrier;
    MultiProducerSequencer<std::size_t> sequencer{readBarrier, bufferSize};

    thread_pool tp{3};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    any_io_executor executorC = tp.get_executor();
    co_spawn(executorA, batch_producer(sequencer, buffer, iterationCount, batchSize), detached);
    co_spawn(executorB, batch_producer(sequencer, buffer, iterationCount, batchSize), detached);

    std::uint64_t result;
    co_spawn(executorC, consumer(sequencer, readBarrier, buffer, producerCount),
             [&](std::exception_ptr, std::uint64_t val) { result = val; } );
    tp.join();

    constexpr std::uint64_t expectedResult =
        producerCount * static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result == expectedResult);
}

BOOST_AUTO_TEST_SUITE_END() // single_consumer

BOOST_AUTO_TEST_SUITE(two_consumers_parallel)

BOOST_AUTO_TEST_CASE(two_producer_single)
{
    constexpr std::size_t bufferSize = 16384;
    std::uint64_t buffer[bufferSize];

    constexpr std::size_t iterationCount = 1'000'000;
    constexpr unsigned producerCount = 2;

    SequenceBarrier<std::size_t> readBarrier1;
    SequenceBarrier<std::size_t> readBarrier2;
    SequenceBarrierGroup<std::size_t> readBarrierGroup{{readBarrier1, readBarrier2}};
    using Sequencer = MultiProducerSequencer<std::size_t, SequenceTraits<std::size_t>, SequenceBarrierGroup<std::size_t>>;
    Sequencer sequencer{readBarrierGroup, bufferSize};

    thread_pool tp{4};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    any_io_executor executorC = tp.get_executor();
    any_io_executor executorD = tp.get_executor();
    co_spawn(executorA, one_at_a_time_producer(sequencer, buffer, iterationCount), detached);
    co_spawn(executorB, one_at_a_time_producer(sequencer, buffer, iterationCount), detached);

    std::uint64_t result1, result2;
    co_spawn(executorC, consumer(sequencer, readBarrier1, buffer, producerCount),
             [&](std::exception_ptr, std::uint64_t val) { result1 = val; } );
    co_spawn(executorD, consumer(sequencer, readBarrier2, buffer, producerCount),
             [&](std::exception_ptr, std::uint64_t val) { result2 = val; } );
    tp.join();

    constexpr std::uint64_t expectedResult =
        producerCount * static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result1 == expectedResult);
    BOOST_TEST(result2 == expectedResult);
}

BOOST_AUTO_TEST_CASE(two_producer_batch)
{
    constexpr std::size_t bufferSize = 16384;
    std::uint64_t buffer[bufferSize];

    constexpr std::size_t iterationCount = 1'000'000;
    constexpr unsigned producerCount = 2;
    constexpr std::size_t batchSize = 10;

    SequenceBarrier<std::size_t> readBarrier1;
    SequenceBarrier<std::size_t> readBarrier2;
    SequenceBarrierGroup<std::size_t> readBarrierGroup{{readBarrier1, readBarrier2}};
    using Sequencer = MultiProducerSequencer<std::size_t, SequenceTraits<std::size_t>, SequenceBarrierGroup<std::size_t>>;
    Sequencer sequencer{readBarrierGroup, bufferSize};

    thread_pool tp{4};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    any_io_executor executorC = tp.get_executor();
    any_io_executor executorD = tp.get_executor();
    co_spawn(executorA, batch_producer(sequencer, buffer, iterationCount, batchSize), detached);
    co_spawn(executorB, batch_producer(sequencer, buffer, iterationCount, batchSize), detached);

    std::uint64_t result1, result2;
    co_spawn(executorC, consumer(sequencer, readBarrier1, buffer, producerCount),
             [&](std::exception_ptr, std::uint64_t val) { result1 = val; } );
    co_spawn(executorD, consumer(sequencer, readBarrier2, buffer, producerCount),
             [&](std::exception_ptr, std::uint64_t val) { result2 = val; } );
    tp.join();

    constexpr std::uint64_t expectedResult =
        producerCount * static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result1 == expectedResult);
    BOOST_TEST(result2 == expectedResult);
}

BOOST_AUTO_TEST_SUITE_END() // two_consumers_parallel

BOOST_AUTO_TEST_SUITE(two_consumers_unique)

namespace {
awaitable<std::uint64_t> consumer_unique(const MultiProducerSequencer<std::size_t, SequenceTraits<std::size_t>, SequenceBarrierGroup<std::size_t>>& sequencer,
                                         SequenceBarrier<std::size_t>& readBarrier,
                                         const std::uint64_t buffer[],
                                         std::atomic_size_t& nextToRead) {

    using Traits = SequenceTraits<std::size_t>;
    const std::size_t indexMask = sequencer.buffer_size() - 1;

    std::uint64_t sum = 0;

    bool reachedEnd = false;
    std::size_t lastKnownPublished = readBarrier.last_published();
    do
    {
        std::size_t toRead = nextToRead.fetch_add(1);
        if (Traits::difference(toRead, lastKnownPublished) >= sequencer.buffer_size()) {
            // Move consumer barrier if retarded
            readBarrier.publish(toRead - 1);
        }

        std::size_t available = co_await sequencer.wait_until_published(toRead, lastKnownPublished);
        sum += buffer[toRead & indexMask];

        // Zero value is sentinel that indicates the end of the stream.
        reachedEnd = buffer[toRead & indexMask] == 0;

        // Notify that we've finished processing up to 'available'.
        readBarrier.publish(toRead);
        lastKnownPublished = available;
    } while (!reachedEnd);

    co_return sum;
}
} // Anonymous namespace

BOOST_AUTO_TEST_CASE(two_producer_single)
{
    constexpr std::size_t bufferSize = 16384;
    std::uint64_t buffer[bufferSize];

    constexpr std::size_t iterationCount = 1'000'000;
    constexpr unsigned producerCount = 2;

    SequenceBarrier<std::size_t> readBarrier1;
    SequenceBarrier<std::size_t> readBarrier2;
    SequenceBarrierGroup<std::size_t> readBarrierGroup{{readBarrier1, readBarrier2}};
    using Sequencer = MultiProducerSequencer<std::size_t, SequenceTraits<std::size_t>, SequenceBarrierGroup<std::size_t>>;
    Sequencer sequencer{readBarrierGroup, bufferSize};

    thread_pool tp{4};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    any_io_executor executorC = tp.get_executor();
    any_io_executor executorD = tp.get_executor();
    co_spawn(executorA, one_at_a_time_producer(sequencer, buffer, iterationCount), detached);
    co_spawn(executorB, one_at_a_time_producer(sequencer, buffer, iterationCount), detached);

    std::atomic_size_t nextToRead = 0;
    std::uint64_t result1, result2;
    co_spawn(executorC, consumer_unique(sequencer, readBarrier1, buffer, nextToRead),
             [&](std::exception_ptr, std::uint64_t val) { result1 = val; } );
    co_spawn(executorD, consumer_unique(sequencer, readBarrier2, buffer, nextToRead),
             [&](std::exception_ptr, std::uint64_t val) { result2 = val; } );
    tp.join();

    constexpr std::uint64_t expectedResult =
        producerCount * static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result1 + result2 == expectedResult);
}

BOOST_AUTO_TEST_CASE(two_producer_batch)
{
    constexpr std::size_t bufferSize = 16384;
    std::uint64_t buffer[bufferSize];

    constexpr std::size_t iterationCount = 1'000'000;
    constexpr unsigned producerCount = 2;
    constexpr std::size_t batchSize = 10;

    SequenceBarrier<std::size_t> readBarrier1;
    SequenceBarrier<std::size_t> readBarrier2;
    SequenceBarrierGroup<std::size_t> readBarrierGroup{{readBarrier1, readBarrier2}};
    using Sequencer = MultiProducerSequencer<std::size_t, SequenceTraits<std::size_t>, SequenceBarrierGroup<std::size_t>>;
    Sequencer sequencer{readBarrierGroup, bufferSize};

    thread_pool tp{4};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    any_io_executor executorC = tp.get_executor();
    any_io_executor executorD = tp.get_executor();
    co_spawn(executorA, batch_producer(sequencer, buffer, iterationCount, batchSize), detached);
    co_spawn(executorB, batch_producer(sequencer, buffer, iterationCount, batchSize), detached);

    std::atomic_size_t nextToRead = 0;
    std::uint64_t result1, result2;
    co_spawn(executorC, consumer_unique(sequencer, readBarrier1, buffer, nextToRead),
             [&](std::exception_ptr, std::uint64_t val) { result1 = val; } );
    co_spawn(executorD, consumer_unique(sequencer, readBarrier2, buffer, nextToRead),
             [&](std::exception_ptr, std::uint64_t val) { result2 = val; } );
    tp.join();

    constexpr std::uint64_t expectedResult =
        producerCount * static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result1 + result2 == expectedResult);
}

BOOST_AUTO_TEST_SUITE_END() // two_consumers_unique

BOOST_AUTO_TEST_SUITE_END() // tests_MultiProducerSequencer

} // namespace boost::asio::awaitable_ext::test
