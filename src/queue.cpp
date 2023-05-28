#include "multi_producer_sequencer.h"
#include "single_producer_sequencer.h"

namespace boost::asio::awaitable_ext {

template class SequenceTraits<std::size_t>;
template class SequenceRange<std::size_t>;
template class SequenceBarrier<std::size_t>;
template class MultiProducerSequencer<std::size_t>;
template class SingleProducerSequencer<std::size_t>;

} // namespace boost::asio::awaitable_ext
