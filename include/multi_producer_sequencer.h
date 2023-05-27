#pragma once

#include "sequence_range.h"
#include "sequence_barrier.h"
#include "is_sequence_barrier.h"

namespace boost::asio::awaitable_ext {

namespace detail {
template<std::unsigned_integral TSequence, typename Traits>
struct MultiProducerSequencerAwaiter
{
    const TSequence targetSequence;
    TSequence lastKnownPublished;
    MultiProducerSequencerAwaiter* next;

    explicit MultiProducerSequencerAwaiter(TSequence targetSequence,
                                           TSequence lastKnownPublished)
        :
        targetSequence{targetSequence},
        lastKnownPublished{lastKnownPublished},
        next{nullptr}
    {}

    awaitable<TSequence> wait() const {
        co_await _event.wait(use_awaitable);
        co_return lastKnownPublished;
    }

    void resume(TSequence published) {
        assert(!Traits::precedes(published, targetSequence));
        lastKnownPublished = published;
        _event.set();
    }

    void cancel() { _event.cancel(); }

private:
    Event _event;
};
} // namespace detail

template<std::unsigned_integral TSequence = std::size_t,
         typename Traits = SequenceTraits<TSequence>,
         IsSequenceBarrier<TSequence> ConsumerBarrier = SequenceBarrier<TSequence, Traits>,
         typename Awaiter = detail::MultiProducerSequencerAwaiter<TSequence, Traits>>
class MultiProducerSequencer
{
public:
    MultiProducerSequencer(const ConsumerBarrier& consumerBarrier,
                           std::size_t bufferSize,
                           TSequence initialSequence = Traits::initial_sequence);
    ~MultiProducerSequencer();

    MultiProducerSequencer(const MultiProducerSequencer&) = delete;
    MultiProducerSequencer& operator=(const MultiProducerSequencer&) = delete;

    /// The size of the circular buffer. This will be a power-of-two.
    std::size_t buffer_size() const noexcept { return _indexMask + 1; }
    std::size_t index_mask() const noexcept { return _indexMask; }

    /// Lookup the last-known-published sequence number after the specified
    /// sequence number.
    TSequence last_published_after(TSequence lastKnownPublished) const;

    /// Wait until the specified target sequence number has been published.
    ///
    /// Returns an awaitable type that when co_awaited will suspend the awaiting
    /// coroutine until the specified 'targetSequence' number and all prior sequence
    /// numbers have been published.
    [[nodiscard]] awaitable<TSequence> wait_until_published(TSequence targetSequence, TSequence lastKnownPublished) const;

    /// Query if there are currently any slots available for claiming.
    ///
    /// Note that this return-value is only approximate if you have multiple producers
    /// since immediately after returning true another thread may have claimed the
    /// last available slot.
    bool any_available() const;

    /// Claim a single slot in the buffer and wait until that slot becomes available.
    ///
    /// Returns an Awaitable type that yields the sequence number of the slot that
    /// was claimed.
    ///
    /// Once the producer has claimed a slot then they are free to write to that
    /// slot within the ring buffer. Once the value has been initialised the item
    /// must be published by calling the .publish() method, passing the sequence
    /// number.
    [[nodiscard]] awaitable<TSequence> claim_one();

    /// Claim a contiguous range of sequence numbers corresponding to slots within
    /// a ring-buffer.
    ///
    /// This will claim at most the specified count of sequence numbers but may claim
    /// fewer if there are only fewer entries available in the buffer. But will claim
    /// at least one sequence number.
    ///
    /// Returns an awaitable that will yield a sequence_range object containing the
    /// sequence numbers that were claimed.
    ///
    /// The caller is responsible for ensuring that they publish every element of the
    /// returned sequence range by calling .publish().
    [[nodiscard]] awaitable<SequenceRange<TSequence, Traits>> claim_up_to(std::size_t count);

    /// Publish the element with the specified sequence number, making it available
    /// to consumers.
    ///
    /// Note that different sequence numbers may be published by different producer
    /// threads out of order. A sequence number will not become available to consumers
    /// until all preceding sequence numbers have also been published.
    ///
    /// \param sequence
    /// The sequence number of the elemnt to publish
    /// This sequence number must have been previously acquired via a call to 'claim_one()'
    /// or 'claim_up_to()'.
    void publish(TSequence sequence);

    /// Publish a contiguous range of sequence numbers, making each of them available
    /// to consumers.
    ///
    /// This is equivalent to calling publish(seq) for each sequence number, seq, in
    /// the specified range, but is more efficient since it only checks to see if
    /// there are coroutines that need to be woken up once.
    void publish(const SequenceRange<TSequence, Traits>& range);

    void close();
    bool is_closed() const;

private:
    const ConsumerBarrier& _consumerBarrier;
    const std::size_t _indexMask;
    const std::unique_ptr<std::atomic<TSequence>[]> _published;

    std::atomic<TSequence> _nextToClaim;
    mutable std::atomic<Awaiter*> _awaiters;
    std::atomic<bool> _isClosed;

    void resume_ready_awaiters();
    void resume_awaiters(Awaiter*, TSequence) const;
    void cancel_awaiters(Awaiter*) const;

protected:
    void add_awaiter(Awaiter*) const;
};

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::MultiProducerSequencer(const ConsumerBarrier& consumerBarrier,
                                                                                            std::size_t bufferSize,
                                                                                            TSequence initialSequence)
    :
    _consumerBarrier{consumerBarrier},
    _indexMask{bufferSize - 1},
    _published{std::make_unique<std::atomic<TSequence>[]>(bufferSize)},
    _nextToClaim{static_cast<TSequence>(initialSequence + 1)},
    _awaiters{nullptr},
    _isClosed{false}
{
    // bufferSize must be a positive power-of-two
    assert(bufferSize > 0 && (bufferSize & (bufferSize - 1)) == 0);
    // but must be no larger than the max diff value.
    using diff_t = typename Traits::difference_type;
    using unsigned_diff_t = std::make_unsigned_t<diff_t>;
    constexpr unsigned_diff_t maxSize = static_cast<unsigned_diff_t>(std::numeric_limits<diff_t>::max());
    assert(bufferSize <= maxSize);

    for (TSequence seq = initialSequence;
         seq > initialSequence - bufferSize;
         seq--)
    {
        _published[seq & _indexMask].store(seq, std::memory_order_relaxed);
    }
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::~MultiProducerSequencer()
{
    assert(_awaiters.load(std::memory_order_relaxed) == nullptr);
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
TSequence MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::last_published_after(TSequence lastKnownPublished) const
{
    TSequence seq = lastKnownPublished + 1;
    while (_published[seq & _indexMask].load(std::memory_order_acquire) == seq) {
        lastKnownPublished = seq++;
    }
    return lastKnownPublished;
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
awaitable<TSequence> MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::wait_until_published(TSequence targetSequence,
                                                                                                               TSequence lastKnownPublished) const
{
    auto cs = co_await this_coro::cancellation_state;
    auto slot = cs.slot();
    if (slot.is_connected()) {
        slot.assign([this](cancellation_type){ const_cast<MultiProducerSequencer*>(this)->close(); });
    }

    auto awaiter = Awaiter{targetSequence, lastKnownPublished};
    add_awaiter(&awaiter);

    // Spawn new coro-thread with dummy cancellation slot and co_await-ed its
    // We explicit call event.close() from awaiter
    TSequence available = co_await co_spawn(
        co_await this_coro::executor,
        awaiter.wait(),
        bind_cancellation_slot(
            cancellation_slot(),
            use_awaitable)
        );

    co_return available;
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
bool MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::any_available() const
{
    return Traits::precedes(
        _nextToClaim.load(std::memory_order_relaxed),
        _consumerBarrier.last_published() + buffer_size());
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
awaitable<TSequence> MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::claim_one()
{
    auto cs = co_await this_coro::cancellation_state;
    auto slot = cs.slot();
    if (slot.is_connected()) {
        slot.assign([this](cancellation_type){ this->close(); });
    }

    const TSequence claimedSequence = _nextToClaim.fetch_add(1, std::memory_order_relaxed);

    // Spawn new coro-thread with dummy cancellation slot and co_await-ed its
    co_await co_spawn(
        co_await this_coro::executor,
        _consumerBarrier.wait_until_published(claimedSequence - buffer_size()),
        bind_cancellation_slot(
            cancellation_slot(),
            use_awaitable)
        );

    co_return claimedSequence;
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
awaitable<SequenceRange<TSequence, Traits>> MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::claim_up_to(std::size_t count)
{
    auto cs = co_await this_coro::cancellation_state;
    auto slot = cs.slot();
    if (slot.is_connected()) {
        slot.assign([this](cancellation_type){ this->close(); });
    }

    count = std::min(count, buffer_size());
    const TSequence first = _nextToClaim.fetch_add(count, std::memory_order_relaxed);
    auto claimedRange = SequenceRange<TSequence, Traits>{first, first + count};

    // Spawn new coro-thread with dummy cancellation slot and co_await-ed its
    co_await co_spawn(
        co_await this_coro::executor,
        _consumerBarrier.wait_until_published(claimedRange.back() - buffer_size()),
        bind_cancellation_slot(
            cancellation_slot(),
            use_awaitable)
        );

    co_return claimedRange;
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
void MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::publish(TSequence sequence)
{
    _published[sequence & _indexMask].store(sequence, std::memory_order_seq_cst);
    resume_ready_awaiters();
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
void MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::publish(const SequenceRange<TSequence, Traits>& range)
{
    assert(!range.empty());

    // Publish all but the first sequence number using relaxed atomics.
    // No consumer should be reading those subsequent sequence numbers until they've seen
    // that the first sequence number in the range is published.
    for (TSequence seq : range.skip(1))
    {
        _published[seq & _indexMask].store(seq, std::memory_order_relaxed);
    }
    // Now publish the first sequence number with seq_cst semantics.
    _published[range.front() & _indexMask].store(range.front(), std::memory_order_seq_cst);

    resume_ready_awaiters();
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
void MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::resume_ready_awaiters()
{
//    Awaiter* awaiters = m_awaiters.load(std::memory_order_seq_cst);
//    if (awaiters == nullptr) {
//        // No awaiters
//        return;
//    }

    // There were some awaiters. Try to acquire the list of waiters with an
    // atomic exchange as we might be racing with other consumers/producers.
    Awaiter* awaiters = _awaiters.exchange(nullptr, std::memory_order_seq_cst);
    if (awaiters == nullptr) {
        // Didn't acquire the list
        // Some other thread is now responsible for resuming them. Our job is done.
        return;
    }

    TSequence lastKnownPublished;

    Awaiter* awaitersToResume;
    Awaiter** awaitersToResumeTail = &awaitersToResume;

    Awaiter* awaitersToRequeue;
    Awaiter** awaitersToRequeueTail = &awaitersToRequeue;

    bool isClosed = false;

    do
    {
        lastKnownPublished = last_published_after(awaiters->lastKnownPublished);

        // First scan the list of awaiters and split them into 'requeue' and 'resume' lists.
        auto minDiff = std::numeric_limits<typename Traits::difference_type>::max();
        do
        {
            auto diff = Traits::difference(awaiters->targetSequence, lastKnownPublished);
            if (diff > 0)
            {
                // Not ready yet.
                minDiff = std::min(diff, minDiff);
                awaiters->lastKnownPublished = lastKnownPublished;
                *awaitersToRequeueTail = awaiters;
                awaitersToRequeueTail = &(awaiters->next);
            }
            else
            {
                *awaitersToResumeTail = awaiters;
                awaitersToResumeTail = &(awaiters->next);
            }
            awaiters = awaiters->next;
        } while (awaiters != nullptr);

        // Null-terinate the requeue list
        *awaitersToRequeueTail = nullptr;

        if (awaitersToRequeue != nullptr)
        {
            // Requeue the waiters that are not ready yet.
            Awaiter* oldHead = nullptr;
            while (!_awaiters.compare_exchange_weak(oldHead,
                                                    awaitersToRequeue,
                                                    std::memory_order_seq_cst,
                                                    std::memory_order_relaxed))
            {
                *awaitersToRequeueTail = oldHead;
            }

            // Reset the awaitersToRequeue list
            awaitersToRequeueTail = &awaitersToRequeue;

            const TSequence earliestTargetSequence = lastKnownPublished + minDiff;

            // Now we need to check again to see if any of the waiters we just enqueued
            // is now satisfied by a concurrent call to publish().
            //
            // We need to be a bit more careful here since we are no longer holding any
            // awaiters and so producers/consumers may advance the sequence number arbitrarily
            // far. If the sequence number advances more than buffer_size() ahead of the
            // earliestTargetSequence then the m_published[] array may have sequence numbers
            // that have advanced beyond earliestTargetSequence, potentially even wrapping
            // sequence numbers around to then be preceding where they were before. If this
            // happens then we don't need to worry about resuming any awaiters that were waiting
            // for 'earliestTargetSequence' since some other thread has already resumed them.
            // So the only case we need to worry about here is when all m_published entries for
            // sequence numbers in range [lastKnownPublished + 1, earliestTargetSequence] have
            // published sequence numbers that match the range.
            TSequence seq = lastKnownPublished + 1;
            while (_published[seq & _indexMask].load(std::memory_order_seq_cst) == seq)
            {
                lastKnownPublished = seq;
                if (seq == earliestTargetSequence)
                {
                    // At least one of the awaiters we just published is now satisfied.
                    // Reacquire the list of awaiters and continue around the outer loop.
                    awaiters = _awaiters.exchange(nullptr, std::memory_order_acquire);
                    break;
                }
                ++seq;
            }

            isClosed = _isClosed.load(std::memory_order_seq_cst);
            if (isClosed && awaiters == nullptr) {
                awaiters = _awaiters.exchange(nullptr, std::memory_order_acquire);
            }
        }
    } while (awaiters != nullptr && !isClosed);

    // Null-terminate list of awaiters to resume.
    *awaitersToResumeTail = nullptr;

    resume_awaiters(awaitersToResume, lastKnownPublished);
    if (isClosed) {
        cancel_awaiters(awaiters);
    }
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
void MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::add_awaiter(Awaiter* awaiter) const
{
    TSequence targetSequence = awaiter->targetSequence;
    TSequence lastKnownPublished = awaiter->lastKnownPublished;

    Awaiter* awaitersToEnqueue = awaiter;
    Awaiter** awaitersToEnqueueTail = &(awaiter->next);

    Awaiter* awaitersToResume;
    Awaiter** awaitersToResumeTail = &awaitersToResume;

    bool isClosed = false;

    do
    {
        // Enqueue the awaiters.
        {
            Awaiter* oldHead = _awaiters.load(std::memory_order_relaxed);
            do
            {
                *awaitersToEnqueueTail = oldHead;
            } while (!_awaiters.compare_exchange_weak(
                oldHead,
                awaitersToEnqueue,
                std::memory_order_seq_cst,
                std::memory_order_relaxed));
        }

        // Reset list of waiters
        awaitersToEnqueueTail = &awaitersToEnqueue;

        // Check to see if the last-known published sequence number has advanced
        // while we were enqueuing the awaiters. Need to use seq_cst memory order
        // here to ensure that if there are concurrent calls to publish() that would
        // wake up any of the awaiters we just enqueued that either we will see their
        // write to m_published slots or they will see our write to m_awaiters.
        //
        // Note also, that we are assuming that the last-known published sequence is
        // not going to advance more than buffer_size() ahead of targetSequence since
        // there is at least one consumer that won't be resumed and so thus can't
        // publish the sequence number it's waiting for to its sequence_barrier and so
        // producers won't be able to claim its slot in the buffer.
        //
        // TODO: Check whether we can weaken the memory order here to just use 'seq_cst' on the
        // first .load() and then use 'acquire' on subsequent .load().
        while (_published[(lastKnownPublished + 1) & _indexMask].load(std::memory_order_seq_cst) == (lastKnownPublished + 1))
        {
            ++lastKnownPublished;
        }
        isClosed = _isClosed.load(std::memory_order_seq_cst);

        if (!Traits::precedes(lastKnownPublished, targetSequence) || isClosed)
        {
            // At least one awaiter we just enqueued has now been satisified.
            // To ensure it is woken up we need to reacquire the list of awaiters and resume
            Awaiter* awaiters = _awaiters.exchange(nullptr, std::memory_order_acquire);

            auto minDiff = std::numeric_limits<typename Traits::difference_type>::max();
            while (awaiters != nullptr)
            {
                auto diff = Traits::difference(awaiters->targetSequence, lastKnownPublished);
                if (diff > 0)
                {
                    // Not yet ready.
                    minDiff = std::min(diff, minDiff);
                    awaiters->lastKnownPublished = lastKnownPublished;
                    *awaitersToEnqueueTail = awaiters;
                    awaitersToEnqueueTail = &(awaiters->next);
                }
                else
                {
                    // Now ready.
                    *awaitersToResumeTail = awaiters;
                    awaitersToResumeTail = &(awaiters->next);
                }
                awaiters = awaiters->next;
            }

            // Calculate the earliest sequence number that any awaiters in the
            // awaitersToEnqueue list are waiting for. We'll use this next time
            // around the loop.
            targetSequence = static_cast<TSequence>(lastKnownPublished + minDiff);
        }

        // Null-terminate list of awaiters to enqueue.
        *awaitersToEnqueueTail = nullptr;

    } while (awaitersToEnqueue != nullptr && !isClosed);

    // Null-terminate awaiters to resume.
    *awaitersToResumeTail = nullptr;

    // Finally, resume any awaiters we've found that are ready to go.
    resume_awaiters(awaitersToResume, lastKnownPublished);
    if (isClosed) {
        cancel_awaiters(awaitersToEnqueue);
    }
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
void MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::close()
{
    _isClosed.exchange(true, std::memory_order_seq_cst);
    Awaiter* awaiters = _awaiters.exchange(nullptr, std::memory_order_seq_cst);
    cancel_awaiters(awaiters);
    const_cast<ConsumerBarrier&>(_consumerBarrier).close();
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
bool MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::is_closed() const
{
    return _isClosed.load(std::memory_order_relaxed) || _consumerBarrier.is_closed();
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
void MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::resume_awaiters(Awaiter* awaiters, TSequence published) const
{
    while (awaiters != nullptr)
    {
        Awaiter* next = awaiters->next;
        awaiters->resume(published);
        awaiters = next;
    }
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier, typename Awaiter>
void MultiProducerSequencer<TSequence, Traits, ConsumerBarrier, Awaiter>::cancel_awaiters(Awaiter* awaiters) const
{
    while (awaiters != nullptr)
    {
        Awaiter* next = awaiters->next;
        awaiters->cancel();
        awaiters = next;
    }
}

} // namespace boost::asio::awaitable_ext
