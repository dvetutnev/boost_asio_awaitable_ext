#pragma once

#include "event.h"
#include "sequence_traits.h"

#include <concepts>

namespace boost::asio::awaitable_ext {

namespace detail {
template<std::unsigned_integral TSequence, typename Traits>
struct Awaiter
{
    const TSequence targetSequence;
    Awaiter* next;

    explicit Awaiter(TSequence s) : targetSequence{s}, next{nullptr} {}

    awaitable<TSequence> wait(any_io_executor executor) const {
        co_await _event.wait(executor);
        co_return _published;
    }

    void resume(TSequence published) {
        assert(!Traits::precedes(published, targetSequence));
        _published = published;
        _event.set();
    }

private:
    Event _event;
    TSequence _published;
};
} // namespace detail

template<std::unsigned_integral TSequence = std::size_t,
         typename Traits = SequenceTraits<TSequence>,
         typename Awaiter = detail::Awaiter<TSequence, Traits>>
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

protected:
    void add_awaiter(Awaiter*) const;

private:
    std::atomic<TSequence> _lastPublished;
    mutable std::atomic<Awaiter*> _awaiters;
};

template<std::unsigned_integral TSequence, typename Traits, typename Awaiter>
SequenceBarrier<TSequence, Traits, Awaiter>::SequenceBarrier(TSequence initialSequence)
    :
    _lastPublished{initialSequence},
    _awaiters{nullptr}
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
    any_io_executor executor = co_await this_coro::executor;
    co_return co_await wait_until_published(targetSequence, executor);
}

template<std::unsigned_integral TSequence, typename Traits, typename Awaiter>
awaitable<TSequence> SequenceBarrier<TSequence, Traits, Awaiter>::wait_until_published(TSequence targetSequence, any_io_executor executor) const
{
    TSequence lastPublished = last_published();
    if (!Traits::precedes(lastPublished, targetSequence)) {
        co_return lastPublished;
    }

    auto awaiter = Awaiter{targetSequence};
    add_awaiter(&awaiter);
    lastPublished = co_await awaiter.wait(executor);
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
            std::memory_order_release,
            std::memory_order_relaxed))
        {
            *awaitersToRequeueTail = oldHead;
        }
    }

    while (awaitersToResume)
    {
        Awaiter* next = awaitersToResume->next;
        awaitersToResume->resume(sequence);
        awaitersToResume = next;
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
        if (Traits::precedes(lastKnownPublished, targetSequence))
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

    } while (awaitersToRequeue);

    // Null-terminate the list of awaiters to resume
    *awaitersToResumeTail = nullptr;

    // Resume the awaiters that are ready
    while (awaitersToResume != nullptr)
    {
        auto* next = awaitersToResume->next;
        awaitersToResume->resume(lastKnownPublished);
        awaitersToResume = next;
    }
}

} // namespace boost::asio::awaitable_ext
