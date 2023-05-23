#pragma once

#include "event.h"
#include "sequence_traits.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>

#include <concepts>

namespace boost::asio::awaitable_ext {

namespace detail {
template<std::unsigned_integral TSequence, typename Traits>
struct SequenceBarrierAwaiter
{
    const TSequence targetSequence;
    SequenceBarrierAwaiter* next;

    explicit SequenceBarrierAwaiter(TSequence s) : targetSequence{s}, next{nullptr} {}

    awaitable<TSequence> wait() const {
        co_await _event.wait(use_awaitable);
        co_return _published;
    }

    void resume(TSequence published) {
        assert(!Traits::precedes(published, targetSequence));
        _published = published;
        _event.set();
    }

    void cancel() { _event.cancel(); }

private:
    Event _event;
    TSequence _published;
};
} // namespace detail

template<std::unsigned_integral TSequence = std::size_t,
         typename Traits = SequenceTraits<TSequence>,
         typename Awaiter = detail::SequenceBarrierAwaiter<TSequence, Traits>>
class SequenceBarrier
{
public:
    SequenceBarrier(TSequence initialSequence = Traits::initial_sequence);
    ~SequenceBarrier();

    SequenceBarrier(const SequenceBarrier&) = delete;
    SequenceBarrier& operator=(const SequenceBarrier&) = delete;

    TSequence last_published() const;
    [[nodiscard]] awaitable<TSequence> wait_until_published(TSequence) const;
    [[nodiscard]] awaitable<TSequence> wait_until_published(TSequence, any_io_executor) const;

    void publish(TSequence);

    void close();
    bool is_closed() const;

protected:
    void add_awaiter(Awaiter*) const;

private:
    std::atomic<TSequence> _lastPublished;
    mutable std::atomic<Awaiter*> _awaiters;
    std::atomic<bool> _isClosed;

    void resume_awaiters(Awaiter*, TSequence) const;
    void cancel_awaiters(Awaiter*) const;
};

template<std::unsigned_integral TSequence, typename Traits, typename Awaiter>
SequenceBarrier<TSequence, Traits, Awaiter>::SequenceBarrier(TSequence initialSequence)
    :
    _lastPublished{initialSequence},
    _awaiters{nullptr},
    _isClosed{false}
{}

template<std::unsigned_integral TSequence, typename Traits, typename Awaiter>
SequenceBarrier<TSequence, Traits, Awaiter>::~SequenceBarrier()
{
    assert(_awaiters.load(std::memory_order_relaxed) == nullptr);
}

template<std::unsigned_integral TSequence, typename Traits, typename Awaiter>
TSequence SequenceBarrier<TSequence, Traits, Awaiter>::last_published() const
{
    return _lastPublished.load(std::memory_order_acquire);
}

template<std::unsigned_integral TSequence, typename Traits, typename Awaiter>
awaitable<TSequence> SequenceBarrier<TSequence, Traits, Awaiter>::wait_until_published(TSequence targetSequence) const
{
    TSequence lastPublished = last_published();
    if (!Traits::precedes(lastPublished, targetSequence)) {
        co_return lastPublished;
    }

    auto cs = co_await this_coro::cancellation_state;
    auto slot = cs.slot();
    if (slot.is_connected()) {
        slot.assign([this](cancellation_type){ const_cast<SequenceBarrier*>(this)->close(); });
    }

    auto awaiter = Awaiter{targetSequence};
    add_awaiter(&awaiter);

    // Spawn new coro-thread with dummy cancellation slot and co_await-ed its
    // We explicit call event.close() from awaiter
    lastPublished = co_await co_spawn(
        co_await this_coro::executor,
        awaiter.wait(),
        bind_cancellation_slot(
            cancellation_slot(),
            use_awaitable)
        );

    co_return lastPublished;
}

template<std::unsigned_integral TSequence, typename Traits, typename Awaiter>
void SequenceBarrier<TSequence, Traits, Awaiter>::publish(TSequence sequence)
{
    _lastPublished.store(sequence, std::memory_order_seq_cst);

    // Cheaper check to see if there are any awaiting coroutines.
//    Awaiter* awaiters = _awaiters.load();
//    if (!awaiters) {
//        return;
//    }

    Awaiter* awaiters;

    awaiters = _awaiters.exchange(nullptr, std::memory_order_seq_cst);
    if (!awaiters) {
        return;
    }

    // Check the list of awaiters for ones that are now satisfied by the sequence number
    // we just published. Awaiters are added to either the 'awaitersToResume' list or to
    // the 'awaitersToRequeue' list.
    Awaiter* awaitersToRequeue;
    Awaiter** awaitersToRequeueTail = &awaitersToRequeue;

    Awaiter* awaitersToResume;
    Awaiter** awaitersToResumeTail = &awaitersToResume;

    do
    {
        if (Traits::precedes(sequence, awaiters->targetSequence))
        {
            // Target sequence not reached. Append to 'requeue' list.
            *awaitersToRequeueTail = awaiters;
            awaitersToRequeueTail = &(awaiters->next);
        }
        else
        {
            // Target sequence reached. Append to 'resume' list.
            *awaitersToResumeTail = awaiters;
            awaitersToResumeTail = &(awaiters->next);
        }
        awaiters = awaiters->next;
    } while (awaiters);

    // null-terminate the two lists.
    *awaitersToRequeueTail = nullptr;
    *awaitersToResumeTail = nullptr;

    if (awaitersToRequeue)
    {
        Awaiter* oldHead = nullptr;
        while (!_awaiters.compare_exchange_weak(
            oldHead,
            awaitersToRequeue,
            std::memory_order_seq_cst,
            std::memory_order_relaxed))
        {
            *awaitersToRequeueTail = oldHead;
        }
    }

    resume_awaiters(awaitersToResume, sequence);

    if (_isClosed.load(std::memory_order_seq_cst))
    {
        awaiters = _awaiters.exchange(nullptr, std::memory_order_seq_cst);
        cancel_awaiters(awaiters);
    }
}

template<std::unsigned_integral TSequence, typename Traits, typename Awaiter>
void SequenceBarrier<TSequence, Traits, Awaiter>::add_awaiter(Awaiter* awaiter) const
{
    TSequence targetSequence = awaiter->targetSequence;
    Awaiter* awaitersToRequeue = awaiter;
    Awaiter** awaitersToRequeueTail = &(awaiter->next);

    TSequence lastKnownPublished;
    Awaiter* awaitersToResume;
    Awaiter** awaitersToResumeTail = &awaitersToResume;

    bool isClosed = false;

    do
    {
        // Enqueue the awaiter(s)
        {
            auto* oldHead = _awaiters.load(std::memory_order_relaxed);
            do
            {
                *awaitersToRequeueTail = oldHead;
            } while (!_awaiters.compare_exchange_weak(
                oldHead,
                awaitersToRequeue,
                std::memory_order_seq_cst,
                std::memory_order_relaxed));
        }

        // Check that the sequence we were waiting for wasn't published while
        // we were enqueueing the waiter.
        // This needs to be seq_cst memory order to ensure that in the case that the producer
        // publishes a new sequence number concurrently with this call that we either see
        // their write to m_lastPublished after enqueueing our awaiter, or they see our
        // write to m_awaiters after their write to m_lastPublished.
        lastKnownPublished = _lastPublished.load(std::memory_order_seq_cst);
        isClosed = _isClosed.load(std::memory_order_seq_cst);
        if (Traits::precedes(lastKnownPublished, targetSequence) && !isClosed)
        {
            // None of the the awaiters we enqueued have been satisfied yet.
            break;
        }

        // Reset the requeue list to empty
        awaitersToRequeueTail = &awaitersToRequeue;

        // At least one of the awaiters we just enqueued is now satisfied by a concurrently
        // published sequence number. The producer thread may not have seen our write to m_awaiters
        // so we need to try to re-acquire the list of awaiters to ensure that the waiters that
        // are now satisfied are woken up.
        auto* awaiters = _awaiters.exchange(nullptr, std::memory_order_acquire);

        auto minDiff = std::numeric_limits<typename Traits::difference_type>::max();

        while (awaiters)
        {
            const auto diff = Traits::difference(awaiters->targetSequence, lastKnownPublished);
            if (diff > 0)
            {
                *awaitersToRequeueTail = awaiters;
                awaitersToRequeueTail = &(awaiters->next);
                minDiff = std::min(diff, minDiff);
            }
            else
            {
                *awaitersToResumeTail = awaiters;
                awaitersToResumeTail = &(awaiters->next);
            }

            awaiters = awaiters->next;
        }

        // Null-terminate the list of awaiters to requeue.
        *awaitersToRequeueTail = nullptr;

        // Calculate the earliest target sequence required by any of the awaiters to requeue.
        targetSequence = static_cast<TSequence>(lastKnownPublished + minDiff);

    } while (awaitersToRequeue != nullptr && !isClosed);

    // Null-terminate the list of awaiters to resume
    *awaitersToResumeTail = nullptr;

    // Resume the awaiters that are ready
    resume_awaiters(awaitersToResume, lastKnownPublished);
    if (isClosed) {
        cancel_awaiters(awaitersToRequeue);
    }
}

template<std::unsigned_integral TSequence, typename Traits, typename Awaiter>
void SequenceBarrier<TSequence, Traits, Awaiter>::close()
{
    _isClosed.exchange(true, std::memory_order_seq_cst);
    Awaiter* awaiters = _awaiters.exchange(nullptr, std::memory_order_seq_cst);
    cancel_awaiters(awaiters);
}

template<std::unsigned_integral TSequence, typename Traits, typename Awaiter>
bool SequenceBarrier<TSequence, Traits, Awaiter>::is_closed() const
{
    return _isClosed.load(std::memory_order_relaxed);
}

template<std::unsigned_integral TSequence, typename Traits, typename Awaiter>
void SequenceBarrier<TSequence, Traits, Awaiter>::resume_awaiters(Awaiter* awaiters, TSequence published) const
{
    while (awaiters != nullptr)
    {
        Awaiter* next = awaiters->next;
        awaiters->resume(published);
        awaiters = next;
    }
}

template<std::unsigned_integral TSequence, typename Traits, typename Awaiter>
void SequenceBarrier<TSequence, Traits, Awaiter>::cancel_awaiters(Awaiter* awaiters) const
{
    while (awaiters != nullptr)
    {
        Awaiter* next = awaiters->next;
        awaiters->cancel();
        awaiters = next;
    }
}

} // namespace boost::asio::awaitable_ext
