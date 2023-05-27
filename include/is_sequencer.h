#include "sequence_range.h"

#include <boost/asio/awaitable.hpp>

#include <concepts>

namespace boost::asio::awaitable_ext {

template<typename Sequencer, typename TSequence>
concept IsSequencerBase = requires(Sequencer b, TSequence s, SequenceRange<TSequence> r) {
    { b.claim_one() } -> std::same_as<awaitable<TSequence>>;
    { b.claim_up_to(s) } -> std::same_as<awaitable<SequenceRange<TSequence>>>;
    { b.publish(s) };
    { b.publish(r) };
    { b.close() };
};

template<typename Sequencer, typename TSequence>
concept IsSingleProducer = requires(Sequencer b, TSequence s) {
    { b.wait_until_published(s) } -> std::same_as<awaitable<TSequence>>;
};

template<typename Sequencer, typename TSequence>
concept IsMultiProducer = requires(Sequencer b, TSequence s, TSequence l) {
    { b.wait_until_published(s, l) } -> std::same_as<awaitable<TSequence>>;
};

template<typename Sequencer, typename TSequence>
concept IsSequencer = IsSequencerBase<Sequencer, TSequence>
                      && (IsSingleProducer<Sequencer, TSequence>
                          || IsMultiProducer<Sequencer, TSequence>);

} // namespace boost::asio::awaitable_ext
