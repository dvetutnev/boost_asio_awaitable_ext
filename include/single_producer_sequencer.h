#pragma once

#include "sequence_barrier.h"
#include "sequence_range.h"

namespace boost::asio::awaitable_ext {

template<typename Barrier, typename TSequence>
concept IsSequenceBarrier = requires(Barrier b, TSequence s) {
    { b.wait_until_published(s) } -> std::same_as<awaitable<TSequence>>;
};

template<std::unsigned_integral TSequence = std::size_t,
         typename Traits = SequenceTraits<TSequence>,
         IsSequenceBarrier<TSequence> ConsumerBarrier = SequenceBarrier<TSequence, Traits>>
class SingleProducerSequencer
{
public:
    SingleProducerSequencer(const ConsumerBarrier& consumerBarrier,
                            std::size_t bufferSize,
                            TSequence initialSequence = Traits::initial_sequence);

    SingleProducerSequencer(const SingleProducerSequencer&) = delete;
    SingleProducerSequencer& operator=(const SingleProducerSequencer&) = delete;

    [[nodiscard]] awaitable<TSequence> claim_one();
    [[nodiscard]] awaitable<SequenceRange<TSequence, Traits>> claim_up_to(std::size_t);
    void publish(TSequence);
    void publish(const SequenceRange<TSequence, Traits>&);

    TSequence last_published() const;
    [[nodiscard]] awaitable<TSequence> wait_until_published(TSequence) const;

private:
    const ConsumerBarrier& _consumerBarrier;
    const std::size_t _bufferSize;

    TSequence _nextToClaim;

    SequenceBarrier<TSequence, Traits> _producerBarrier;
};

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier>
SingleProducerSequencer<TSequence, Traits, ConsumerBarrier>::SingleProducerSequencer(const ConsumerBarrier& consumerBarrier,
                                                                                     std::size_t bufferSize,
                                                                                     TSequence initialSequence)
    :
    _consumerBarrier{consumerBarrier},
    _bufferSize{bufferSize},
    _nextToClaim{initialSequence + 1},
    _producerBarrier{initialSequence}
{
    // bufferSize must be a positive power-of-two
    assert(bufferSize > 0 && (bufferSize & (bufferSize - 1)) == 0);
    // but must be no larger than the max diff value.
    using diff_t = typename Traits::difference_type;
    using unsigned_diff_t = std::make_unsigned_t<diff_t>;
    constexpr unsigned_diff_t maxSize = static_cast<unsigned_diff_t>(std::numeric_limits<diff_t>::max());
    assert(bufferSize <= maxSize);
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier>
awaitable<TSequence> SingleProducerSequencer<TSequence, Traits, ConsumerBarrier>::claim_one()
{
    const auto nextToWrite = static_cast<TSequence>(_nextToClaim - _bufferSize);
    co_await _consumerBarrier.wait_until_published(nextToWrite);
    co_return _nextToClaim++;
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier>
awaitable<SequenceRange<TSequence, Traits>> SingleProducerSequencer<TSequence, Traits, ConsumerBarrier>::claim_up_to(std::size_t count)
{
    const auto nextToWrite = static_cast<TSequence>(_nextToClaim - _bufferSize);
    const TSequence lastAvailableSequence =
        static_cast<TSequence>(co_await _consumerBarrier.wait_until_published(nextToWrite) + _bufferSize);

    const TSequence begin = _nextToClaim;
    const std::size_t availableCount = static_cast<std::size_t>(lastAvailableSequence - begin) + 1;
    const std::size_t countToClaim = std::min(count, availableCount);
    const TSequence end = static_cast<TSequence>(begin + countToClaim);

    _nextToClaim = end;
    co_return SequenceRange<TSequence, Traits>{begin, end};
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier>
void SingleProducerSequencer<TSequence, Traits, ConsumerBarrier>::publish(TSequence sequence)
{
    _producerBarrier.publish(sequence);
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier>
void SingleProducerSequencer<TSequence, Traits, ConsumerBarrier>::publish(const SequenceRange<TSequence, Traits>& range)
{
    publish(range.back());
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier>
TSequence SingleProducerSequencer<TSequence, Traits, ConsumerBarrier>::last_published() const
{
    return _producerBarrier.last_published();
}

template<std::unsigned_integral TSequence, typename Traits, IsSequenceBarrier<TSequence> ConsumerBarrier>
awaitable<TSequence> SingleProducerSequencer<TSequence, Traits, ConsumerBarrier>::wait_until_published(TSequence sequence) const
{
    co_return co_await _producerBarrier.wait_until_published(sequence);
}

} // namespace boost::asio::awaitable_ext
