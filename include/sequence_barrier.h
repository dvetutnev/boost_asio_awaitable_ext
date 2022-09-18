#pragma once

#include "event.h"
#include "sequence_traits.h"

#include <concepts>

namespace boost::asio::awaitable_ext {

template<std::unsigned_integral TSequence = std::size_t,
         typename Traits = SequenceTraits<TSequence>>
class SequenceBarrier
{
public:
    SequenceBarrier(TSequence initialSequence = Traits::initial_sequence);
    ~SequenceBarrier();

    TSequence last_published() const;
    awaitable<TSequence> wait_until_published(TSequence);
    void publish(TSequence);

private:
    struct Awaiter;

    void add_awaiter(Awaiter*);

    std::atomic<TSequence> _lastPublished;
    std::atomic<Awaiter*> _awaiters;
};

template<std::unsigned_integral TSequence, typename Traits>
struct SequenceBarrier<TSequence, Traits>::SequenceBarrier::Awaiter
{
    const TSequence targetSequence;
    Event _event;
    TSequence _published;

    explicit Awaiter(TSequence s) : targetSequence{s} {}

    awaitable<TSequence> wait(any_io_executor executor) {
        co_await _event.wait(executor);
        co_return _published;
    }

    void resume(TSequence published) {
        assert(!Traits::precedes(published, targetSequence));
        _published = published;
        _event.set();
    }
};

template<std::unsigned_integral TSequence, typename Traits>
SequenceBarrier<TSequence, Traits>::SequenceBarrier(TSequence initialSequence)
    :
    _lastPublished{initialSequence},
    _awaiters{nullptr}
{}

template<std::unsigned_integral TSequence, typename Traits>
SequenceBarrier<TSequence, Traits>::~SequenceBarrier()
{
    assert(_awaiters.load(std::memory_order_relaxed) == nullptr);
}

template<std::unsigned_integral TSequence, typename Traits>
TSequence SequenceBarrier<TSequence, Traits>::last_published() const
{
    return _lastPublished.load(std::memory_order_acquire);
}

template<std::unsigned_integral TSequence, typename Traits>
awaitable<TSequence> SequenceBarrier<TSequence, Traits>::wait_until_published(TSequence targetSequence)
{
    TSequence lastPublished = last_published();
    if (!Traits::precedes(lastPublished, targetSequence)) {
        co_return lastPublished;
    }

    auto awaiter = Awaiter{targetSequence};
    add_awaiter(&awaiter);

    any_io_executor executor = co_await this_coro::executor;
    lastPublished = co_await awaiter.wait(executor);

    co_return lastPublished;
}

template<std::unsigned_integral TSequence, typename Traits>
void SequenceBarrier<TSequence, Traits>::publish(TSequence sequence)
{
    _lastPublished = sequence;
    Awaiter* awaiter = _awaiters.load();
    if (!Traits::precedes(sequence, awaiter->targetSequence)) {
        awaiter->resume(sequence);
        _awaiters = nullptr;
    }
}

template<std::unsigned_integral TSequence, typename Traits>
void SequenceBarrier<TSequence, Traits>::add_awaiter(Awaiter* awaiter)
{
    _awaiters = awaiter;
}

} // namespace boost::asio::awaitable_ext
