#pragma once

#include "event.h"
#include "sequence_traits.h"

namespace boost::asio::awaitable_ext {

template<typename TSequence = std::size_t,
         typename Traits = SequenceTraits<TSequence>>
class SequenceBarrier
{
public:
    SequenceBarrier(TSequence initialSequence = Traits::initial_sequence);
    ~SequenceBarrier();

    awaitable<TSequence> wait_until_publish(TSequence);

private:
    class Awaiter;

    std::atomic<TSequence> _lastKnowPublished;
    std::atomic<Awaiter*> _awaiters;
};

template<typename TSequence, typename Traits>
SequenceBarrier<TSequence, Traits>::SequenceBarrier(TSequence initialSequence)
    :
    _lastKnowPublished{initialSequence},
    _awaiters{nullptr}
{}

template<typename TSequence, typename Traits>
SequenceBarrier<TSequence, Traits>::~SequenceBarrier()
{
    assert(_awaiters.load(std::memory_order_relaxed) == nullptr);
}

template<typename TSequence, typename Traits>
awaitable<TSequence> SequenceBarrier<TSequence, Traits>::wait_until_publish(TSequence)
{
    co_return _lastKnowPublished;
}

} // namespace boost::asio::awaitable_ext