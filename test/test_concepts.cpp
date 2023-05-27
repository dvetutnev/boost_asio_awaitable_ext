#include "is_sequence_barrier.h"
#include "is_sequencer.h"
#include "single_producer_sequencer.h"
#include "multi_producer_sequencer.h"

#include <boost/test/unit_test.hpp>

namespace boost::asio::awaitable_ext::test {

BOOST_AUTO_TEST_SUITE(tests_concepts);

BOOST_AUTO_TEST_CASE(is_sequence_barrier)
{
    if constexpr (!IsSequenceBarrier<SequenceBarrier<std::size_t>, std::size_t>) {
        BOOST_FAIL("IsSequenceBarrier<SequenceBarrier<std::size_t>, std::size_t>");
    }
}

BOOST_AUTO_TEST_CASE(is_sequencer)
{
    if constexpr (!IsSequencer<SingleProducerSequencer<std::size_t>, std::size_t>) {
        BOOST_FAIL("IsSequencer<SingleProducerSequencer<std::size_t>, std::size_t>");
    }
    if constexpr (!IsSequencer<MultiProducerSequencer<std::size_t>, std::size_t>) {
        BOOST_FAIL("IsSequencer<MultiYProducerSequencer<std::size_t>, std::size_t>");
    }
}

BOOST_AUTO_TEST_CASE(is_single_producer)
{
    if constexpr (!IsSingleProducer<SingleProducerSequencer<std::size_t>, std::size_t>) {
        BOOST_FAIL("IsSingleProducer<SingleProducerSequencer<std::size_t>, std::size_t>");
    }
    if constexpr (!!IsSingleProducer<MultiProducerSequencer<std::size_t>, std::size_t>) {
        BOOST_FAIL("!IsSingleProducer<MultiProducerSequencer<std::size_t>, std::size_t>");
    }
}

BOOST_AUTO_TEST_CASE(is_multi_producer)
{
    if constexpr (!IsMultiProducer<MultiProducerSequencer<std::size_t>, std::size_t>) {
        BOOST_FAIL("IsMultiProducer<MultiProducerSequencer<std::size_t>, std::size_t>");
    }
    if constexpr (!!IsMultiProducer<SingleProducerSequencer<std::size_t>, std::size_t>) {
        BOOST_FAIL("!IsMultiProducer<SingleProducerSequencer<std::size_t>, std::size_t>");
    }
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace boost::asio::awaitable_ext::test
