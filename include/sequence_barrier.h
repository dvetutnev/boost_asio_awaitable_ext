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
    class Awaiter;

    std::atomic<TSequence> _lastPublished;
    std::atomic<Awaiter*> _awaiters;
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
awaitable<TSequence> SequenceBarrier<TSequence, Traits>::wait_until_published(TSequence)
{
    co_return last_published();
}

template<std::unsigned_integral TSequence, typename Traits>
void SequenceBarrier<TSequence, Traits>::publish(TSequence)
{

}

} // namespace boost::asio::awaitable_ext
