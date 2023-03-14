#pragma once

#include "sequence_barrier.h"

namespace boost::asio::awaitable_ext {

template<std::unsigned_integral TSequence = std::size_t,
         typename Traits = SequenceTraits<TSequence>>
class MultiProducerSequencer
{
public:
    MultiProducerSequencer(const SequenceBarrier<TSequence, Traits>& consumerBarrier,
                           std::size_t bufferSize,
                           TSequence initialSequence = Traits::initial_sequence);

    std::size_t buffer_size() const noexcept { return _indexMask + 1; };


    /// Lookup the last-known-published sequence number after the specified
    /// sequence number.
    TSequence last_published_after(TSequence lastKnownPublished) const;

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

private:
    const SequenceBarrier<TSequence, Traits>& _consumerBarrier;
    const std::size_t _indexMask;
    const std::unique_ptr<std::atomic<TSequence>[]> _published;

    void resume_ready_awaiters();
};

template<std::unsigned_integral TSequence, typename Traits>
MultiProducerSequencer<TSequence, Traits>::MultiProducerSequencer(const SequenceBarrier<TSequence, Traits>& consumerBarrier,
                                                                  std::size_t bufferSize,
                                                                  TSequence initialSequence)
    :
    _consumerBarrier{consumerBarrier},
    _indexMask{bufferSize - 1},
    _published{std::make_unique<std::atomic<TSequence>[]>(bufferSize)}
{
    // bufferSize must be a positive power-of-two
    assert(bufferSize > 0 && (bufferSize & (bufferSize - 1)) == 0);
    // but must be no larger than the max diff value.
    using diff_t = typename Traits::difference_type;
    using unsigned_diff_t = std::make_unsigned_t<diff_t>;
    constexpr unsigned_diff_t maxSize = static_cast<unsigned_diff_t>(std::numeric_limits<diff_t>::max());
    assert(bufferSize <= maxSize);

    TSequence seq = initialSequence - (bufferSize - 1);
    do {
        _published[seq & _indexMask].store(seq, std::memory_order_relaxed);
    } while (seq++ != initialSequence);
}

template<std::unsigned_integral TSequence, typename Traits>
TSequence MultiProducerSequencer<TSequence, Traits>::last_published_after(TSequence lastKnownPublished) const
{
    TSequence seq = lastKnownPublished + 1;
    while (_published[seq & _indexMask].load() == seq) {
        lastKnownPublished = seq++;
    }
    return lastKnownPublished;
}


template<std::unsigned_integral TSequence, typename Traits>
void MultiProducerSequencer<TSequence, Traits>::publish(TSequence sequence)
{
    _published[sequence & _indexMask].store(sequence);
    resume_ready_awaiters();
}

template<std::unsigned_integral TSequence, typename Traits>
void MultiProducerSequencer<TSequence, Traits>::resume_ready_awaiters()
{

}


} // namespace boost::asio::awaitable_ext
