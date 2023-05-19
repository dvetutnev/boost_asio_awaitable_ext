#include "multi_producer_sequencer.h"

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/co_spawn.hpp>

#include <boost/test/unit_test.hpp>

#include <disruptorplus/spin_wait_strategy.hpp>
#include <disruptorplus/sequence_barrier.hpp>
#include <disruptorplus/sequence_barrier_group.hpp>

namespace boost::asio::awaitable_ext::test {

namespace {
using TSequence = disruptorplus::sequence_t;
using Traits = SequenceTraits<TSequence>;

struct MockAwaiter
{
    const TSequence targetSequence;
    TSequence lastKnownPublished;
    MockAwaiter* next;

    MockAwaiter(TSequence targetSequence, TSequence lastKnownPublished)
        :
        targetSequence{targetSequence}, lastKnownPublished{lastKnownPublished},
        next{nullptr}, _published{lastKnownPublished}
    {
        assert(Traits::precedes(lastKnownPublished, targetSequence));
    }

    void resume(disruptorplus::sequence_t published) {
        assert(!Traits::precedes(published, targetSequence));
        lastKnownPublished = published;
        _published.store(published, std::memory_order_release);
        _waitStrategy.signal_all_when_blocking();
    }

    TSequence wait() const {
        const std::atomic<disruptorplus::sequence_t>* const sequences[] = { &_published };
        return _waitStrategy.wait_until_published(targetSequence, 1, sequences);
    }

    void cancel() {}

private:
    std::atomic<TSequence> _published;
    mutable disruptorplus::spin_wait_strategy _waitStrategy;
};

using DummyBarrier = SequenceBarrier<TSequence>;
using BaseSequencer = MultiProducerSequencer<TSequence, Traits, DummyBarrier, MockAwaiter>;
struct Sequencer : BaseSequencer
{
    using BaseSequencer::BaseSequencer;
    using BaseSequencer::add_awaiter;
};
} // Anonymous namespace

BOOST_AUTO_TEST_CASE(MultiProducerSequencer_)
{
    constexpr std::size_t bufferSize = 1024;
    constexpr std::size_t indexMask = bufferSize - 1;
    std::uint64_t buffer[bufferSize];

    DummyBarrier dummyBarrier;
    Sequencer sequencer{dummyBarrier, bufferSize};

    using WaitStrategy = disruptorplus::spin_wait_strategy;
    WaitStrategy waitStrategy;
    disruptorplus::sequence_barrier_group<WaitStrategy> readBarrierGroup{waitStrategy};

    std::atomic<TSequence> nextToClaim = 0;

    auto claim_one = [&]() -> TSequence
    {
        const TSequence claimedSequence = nextToClaim.fetch_add(1, std::memory_order_relaxed);
        readBarrierGroup.wait_until_published(claimedSequence - bufferSize);
        return claimedSequence;
    };

    auto one_at_time_producer = [&](std::uint64_t iterationCount)
    {
        std::uint64_t i = 0;
        while (i < iterationCount)
        {
            auto seq = claim_one();
            buffer[seq & indexMask] = ++i;
            sequencer.publish(seq);
        }

        auto finalSeq = claim_one();
        buffer[finalSeq & indexMask] = 0;
        sequencer.publish(finalSeq);
    };

    auto claim_up_to = [&](std::size_t count) -> SequenceRange<TSequence, Traits>
    {
        count = std::min(count, bufferSize);
        const TSequence first = nextToClaim.fetch_add(count, std::memory_order_relaxed);
        auto claimedRange = SequenceRange<TSequence, Traits>{first, first + count};
        readBarrierGroup.wait_until_published(claimedRange.back() - bufferSize);
        return claimedRange;
    };

    auto batch_producer = [&](std::uint64_t iterationCount, std::size_t maxBatchSize)
    {
        std::uint64_t i = 0;
        while (i < iterationCount)
        {
            const std::size_t batchSize = static_cast<std::size_t>(
                std::min<std::uint64_t>(maxBatchSize, iterationCount - i));
            auto sequences = claim_up_to(batchSize);
            for (auto seq : sequences)
            {
                buffer[seq & indexMask] = ++i;
            }
            sequencer.publish(sequences);
        }

        auto finalSeq = claim_one();
        buffer[finalSeq & indexMask] = 0;
        sequencer.publish(finalSeq);
    };

    using ReadBarrier = disruptorplus::sequence_barrier<WaitStrategy>;
    auto consumer = [&](unsigned producerCount, ReadBarrier& readBarrier, std::uint64_t& sum)
    {
        TSequence nextToRead = 0;
        unsigned endCount = 0;
        do
        {
            auto awaiter = MockAwaiter{nextToRead, nextToRead - 1};
            sequencer.add_awaiter(&awaiter);
            const TSequence available = awaiter.wait();
            do
            {
                const auto& value = buffer[nextToRead & indexMask];
                sum += value;
                const bool isEndOfStream = value == 0;
                endCount += isEndOfStream ? 1 : 0;
            } while (nextToRead++ != available);

            readBarrier.publish(available);
        } while (endCount < producerCount);
    };

    constexpr std::uint64_t iterationCount = 1'000'000;
    constexpr unsigned producerCount = 2;

    ReadBarrier readBarrier1{waitStrategy}, readBarrier2{waitStrategy};
    readBarrierGroup.add(readBarrier1); readBarrierGroup.add(readBarrier2);
    std::uint64_t result1 = 0, result2 = 0;

    thread_pool tp{4};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    any_io_executor executorC = tp.get_executor();
    any_io_executor executorD = tp.get_executor();
    post(executorA, [&](){ consumer(producerCount, readBarrier1, result1); });
    post(executorB, [&](){ consumer(producerCount, readBarrier2, result2); });
    post(executorC, [&](){ one_at_time_producer(iterationCount); });
    post(executorD, [&](){ batch_producer(iterationCount, 17); });
    tp.join();

    constexpr std::uint64_t expectedResult =
        producerCount * static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result1 == expectedResult);
    BOOST_TEST(result2 == expectedResult);
}

namespace {
void close_test(std::size_t count) {
    thread_pool tp{5};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    any_io_executor executorC = tp.get_executor();
    any_io_executor executorD = tp.get_executor();
    any_io_executor executorE = tp.get_executor();

    for (std::size_t i = 0; i < count; i++)
    {
        SequenceBarrier<std::size_t> readBarrier;
        MultiProducerSequencer<std::size_t> sequencer{readBarrier, 1024};

        auto consumer = [&](std::size_t targetSequence) -> awaitable<void> {
            try {
                co_await sequencer.wait_until_published(targetSequence, -1);
            } catch (const system::system_error& ex) {
                assert(ex.code() == error::operation_aborted);
            }
        };

        auto producer = [&]() -> awaitable<void> {
            auto seq = co_await sequencer.claim_one();
            sequencer.publish(seq);
        };

        auto close = [&]() -> awaitable<void> {
            sequencer.close();
            co_return;
        };

        std::atomic_bool consumerDone1{false}, consumerDone2{false}, producerDone1{false}, producerDone2{false}, cancelDone{false};

        auto handler = [](std::atomic_bool& flag)
        {
            return [&flag](std::exception_ptr ex)
            {
                if (ex) std::rethrow_exception(ex);
                flag.store(true, std::memory_order_release);
            };
        };

        co_spawn(executorA, consumer(0), handler(consumerDone1));
        co_spawn(executorB, consumer(2), handler(consumerDone2));
        co_spawn(executorC, producer(), handler(producerDone1));
        co_spawn(executorD, producer(), handler(producerDone2));
        co_spawn(executorE, close(), handler(cancelDone));

        while (!consumerDone1.load(std::memory_order_acquire) ||
               !consumerDone2.load(std::memory_order_acquire) ||
               !producerDone1.load(std::memory_order_acquire) ||
               !producerDone2.load(std::memory_order_acquire) ||
               !cancelDone.load(std::memory_order_acquire))
            ;
    };

    tp.join();
}
} // Anonymous namespace

BOOST_AUTO_TEST_CASE(MultiProducerSequencer_cancel_1) { close_test(1); }
BOOST_AUTO_TEST_CASE(MultiProducerSequencer_cancel_10k) { close_test(10'000); }

} // namespace boost::asio::awaitable_ext::test
