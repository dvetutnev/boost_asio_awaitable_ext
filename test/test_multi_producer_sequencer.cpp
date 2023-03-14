#include "multi_producer_sequencer.h"

#include <boost/test/unit_test.hpp>

namespace boost::asio::awaitable_ext::test {

BOOST_AUTO_TEST_SUITE(tests_MultiProducerSequencer)

BOOST_AUTO_TEST_SUITE(last_published_after)

constexpr std::size_t bufferSize = 16;

BOOST_AUTO_TEST_CASE(initial)
{
    SequenceBarrier<std::uint8_t> consumerBarrier;
    MultiProducerSequencer<std::uint8_t> sequencer{consumerBarrier, bufferSize};

    BOOST_TEST(sequencer.last_published_after(0u) == 0u);
    BOOST_TEST(sequencer.last_published_after(1u) == 1u);
    BOOST_TEST(sequencer.last_published_after(2u) == 2u);
    BOOST_TEST(sequencer.last_published_after(3u) == 3u);
    BOOST_TEST(sequencer.last_published_after(4u) == 4u);
    BOOST_TEST(sequencer.last_published_after(5u) == 5u);
    BOOST_TEST(sequencer.last_published_after(6u) == 6u);
    BOOST_TEST(sequencer.last_published_after(7u) == 7u);
    BOOST_TEST(sequencer.last_published_after(8u) == 8u);
    BOOST_TEST(sequencer.last_published_after(9u) == 9u);
    BOOST_TEST(sequencer.last_published_after(10u) == 10u);
    BOOST_TEST(sequencer.last_published_after(11u) == 11u);
    BOOST_TEST(sequencer.last_published_after(12u) == 12u);
    BOOST_TEST(sequencer.last_published_after(13u) == 13u);
    BOOST_TEST(sequencer.last_published_after(14u) == 14u);
    BOOST_TEST(sequencer.last_published_after(15u) == 15u);
    BOOST_TEST(sequencer.last_published_after(16u) == 16u);
    BOOST_TEST(sequencer.last_published_after(17u) == 17u);
    BOOST_TEST(sequencer.last_published_after(18u) == 18u);
    BOOST_TEST(sequencer.last_published_after(19u) == 19u);
    BOOST_TEST(sequencer.last_published_after(20u) == 20u);
    BOOST_TEST(sequencer.last_published_after(21u) == 21u);

    BOOST_TEST(sequencer.last_published_after(236u) == 236u);
    BOOST_TEST(sequencer.last_published_after(237u) == 237u);
    BOOST_TEST(sequencer.last_published_after(238u) == 238u);
    BOOST_TEST(sequencer.last_published_after(239u) == 255u);
    BOOST_TEST(sequencer.last_published_after(240u) == 255u);
    BOOST_TEST(sequencer.last_published_after(241u) == 255u);
    BOOST_TEST(sequencer.last_published_after(242u) == 255u);
    BOOST_TEST(sequencer.last_published_after(243u) == 255u);
    BOOST_TEST(sequencer.last_published_after(244u) == 255u);
    BOOST_TEST(sequencer.last_published_after(245u) == 255u);
    BOOST_TEST(sequencer.last_published_after(246u) == 255u);
    BOOST_TEST(sequencer.last_published_after(247u) == 255u);
    BOOST_TEST(sequencer.last_published_after(248u) == 255u);
    BOOST_TEST(sequencer.last_published_after(249u) == 255u);
    BOOST_TEST(sequencer.last_published_after(250u) == 255u);
    BOOST_TEST(sequencer.last_published_after(251u) == 255u);
    BOOST_TEST(sequencer.last_published_after(252u) == 255u);
    BOOST_TEST(sequencer.last_published_after(253u) == 255u);
    BOOST_TEST(sequencer.last_published_after(254u) == 255u);
    BOOST_TEST(sequencer.last_published_after(255u) == 255u);
}

BOOST_AUTO_TEST_CASE(_3_0)
{
    SequenceBarrier<std::uint8_t> consumerBarrier;
    MultiProducerSequencer<std::uint8_t> sequencer{consumerBarrier, bufferSize};

    sequencer.publish(3);
    BOOST_TEST(sequencer.last_published_after(255u) == 255u);
    BOOST_TEST(sequencer.last_published_after(0u) == 0u);
    BOOST_TEST(sequencer.last_published_after(1u) == 1u);
    BOOST_TEST(sequencer.last_published_after(2u) == 3u);
    BOOST_TEST(sequencer.last_published_after(3u) == 3u);
    BOOST_TEST(sequencer.last_published_after(4u) == 4u);
    BOOST_TEST(sequencer.last_published_after(5u) == 5u);

    BOOST_TEST(sequencer.last_published_after(15u) == 15u);
    BOOST_TEST(sequencer.last_published_after(16u) == 16u);
    BOOST_TEST(sequencer.last_published_after(17u) == 17u);
    BOOST_TEST(sequencer.last_published_after(18u) == 18u);
    BOOST_TEST(sequencer.last_published_after(19u) == 19u);

    sequencer.publish(0);
    BOOST_TEST(sequencer.last_published_after(255u) == 0);
    BOOST_TEST(sequencer.last_published_after(0u) == 0u);
    BOOST_TEST(sequencer.last_published_after(1u) == 1u);
    BOOST_TEST(sequencer.last_published_after(2u) == 3u);
    BOOST_TEST(sequencer.last_published_after(3u) == 3u);
    BOOST_TEST(sequencer.last_published_after(4u) == 4u);
    BOOST_TEST(sequencer.last_published_after(5u) == 5u);

    BOOST_TEST(sequencer.last_published_after(15u) == 15u);
    BOOST_TEST(sequencer.last_published_after(16u) == 16u);
    BOOST_TEST(sequencer.last_published_after(17u) == 17u);
    BOOST_TEST(sequencer.last_published_after(18u) == 18u);
    BOOST_TEST(sequencer.last_published_after(19u) == 19u);
}

BOOST_AUTO_TEST_CASE(_0_3)
{
    SequenceBarrier<std::uint8_t> consumerBarrier;
    MultiProducerSequencer<std::uint8_t> sequencer{consumerBarrier, bufferSize};

    sequencer.publish(0);
    BOOST_TEST(sequencer.last_published_after(255u) == 0);
    BOOST_TEST(sequencer.last_published_after(0u) == 0u);
    BOOST_TEST(sequencer.last_published_after(1u) == 1u);
    BOOST_TEST(sequencer.last_published_after(2u) == 2u);
    BOOST_TEST(sequencer.last_published_after(3u) == 3u);
    BOOST_TEST(sequencer.last_published_after(4u) == 4u);
    BOOST_TEST(sequencer.last_published_after(5u) == 5u);

    BOOST_TEST(sequencer.last_published_after(15u) == 15u);
    BOOST_TEST(sequencer.last_published_after(16u) == 16u);
    BOOST_TEST(sequencer.last_published_after(17u) == 17u);
    BOOST_TEST(sequencer.last_published_after(18u) == 18u);
    BOOST_TEST(sequencer.last_published_after(19u) == 19u);

    sequencer.publish(3);
    BOOST_TEST(sequencer.last_published_after(255u) == 0u);
    BOOST_TEST(sequencer.last_published_after(0u) == 0u);
    BOOST_TEST(sequencer.last_published_after(1u) == 1u);
    BOOST_TEST(sequencer.last_published_after(2u) == 3u);
    BOOST_TEST(sequencer.last_published_after(3u) == 3u);
    BOOST_TEST(sequencer.last_published_after(4u) == 4u);
    BOOST_TEST(sequencer.last_published_after(5u) == 5u);

    BOOST_TEST(sequencer.last_published_after(15u) == 15u);
    BOOST_TEST(sequencer.last_published_after(16u) == 16u);
    BOOST_TEST(sequencer.last_published_after(17u) == 17u);
    BOOST_TEST(sequencer.last_published_after(18u) == 18u);
    BOOST_TEST(sequencer.last_published_after(19u) == 19u);
}

BOOST_AUTO_TEST_CASE(serial)
{
    SequenceBarrier<std::uint8_t> consumerBarrier;
    MultiProducerSequencer<std::uint8_t> sequencer{consumerBarrier, bufferSize};

    sequencer.publish(2);
    sequencer.publish(3);
    sequencer.publish(4);
    BOOST_TEST(sequencer.last_published_after(255u) == 255u);
    BOOST_TEST(sequencer.last_published_after(0u) == 0u);
    BOOST_TEST(sequencer.last_published_after(1u) == 4u);
    BOOST_TEST(sequencer.last_published_after(2u) == 4u);
    BOOST_TEST(sequencer.last_published_after(3u) == 4u);
    BOOST_TEST(sequencer.last_published_after(4u) == 4u);
    BOOST_TEST(sequencer.last_published_after(5u) == 5u);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

} // namespace boost::asio::awaitable_ext::test
