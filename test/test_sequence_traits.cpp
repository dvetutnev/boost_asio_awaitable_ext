#include "sequence_traits.h"

#include <boost/test/unit_test.hpp>
#include <limits>

BOOST_AUTO_TEST_CASE(SequenceTraits_difference)
{
    using Traits = SequenceTraits<unsigned char>;

    BOOST_TEST(Traits::initial_sequence == 255u);

    BOOST_TEST(Traits::difference(0u, 1u) == -1);
    BOOST_TEST(Traits::difference(0u, 126u) == -126);
    BOOST_TEST(Traits::difference(0u, 127u) == -127);
    BOOST_TEST(Traits::difference(0u, 128u) == -128);

    BOOST_TEST(Traits::difference(0u, 129u) != -129);
    BOOST_TEST(Traits::difference(0u, 129u) == static_cast<Traits::difference_type>(-129));
    BOOST_TEST(Traits::difference(0u, 129u) == 127);

    BOOST_TEST(Traits::difference(0u, 130u) == 126);
    BOOST_TEST(Traits::difference(0u, 131u) == 125);
    BOOST_TEST(Traits::difference(0u, 132u) == 124);
    BOOST_TEST(Traits::difference(0u, 133u) == 123);

    BOOST_TEST(Traits::difference(0u, 254u) == 2);
    BOOST_TEST(Traits::difference(0u, 255u) == 1);

    BOOST_TEST(Traits::difference(255u, 0u) == -1);
    BOOST_TEST(Traits::difference(254u, 0u) == -2);
    BOOST_TEST(Traits::difference(131u, 0u) == -125);
    BOOST_TEST(Traits::difference(130u, 0u) == -126);
    BOOST_TEST(Traits::difference(129u, 0u) == -127);
    BOOST_TEST(Traits::difference(128u, 0u) == -128);

    BOOST_TEST(Traits::difference(127u, 0u) != -129);
    BOOST_TEST(Traits::difference(127u, 0u) == static_cast<Traits::difference_type>(-129));
    BOOST_TEST(Traits::difference(127u, 0u) == 127);

    BOOST_TEST(Traits::difference(126u, 0u) == 126);
    BOOST_TEST(Traits::difference(125u, 0u) == 125);
    BOOST_TEST(Traits::difference(124u, 0u) == 124);
}

BOOST_AUTO_TEST_CASE(SequenceTraits_predeces)
{
    using Traits = SequenceTraits<unsigned char>;

    BOOST_TEST(Traits::initial_sequence == 255u);

    BOOST_TEST(Traits::precedes(0u, 1u) == true);
    BOOST_TEST(Traits::precedes(0u, 126u) == true);
    BOOST_TEST(Traits::precedes(0u, 127u) == true);
    BOOST_TEST(Traits::precedes(0u, 128u) == true);

    BOOST_TEST(Traits::precedes(0u, 129u) == false);
    BOOST_TEST(Traits::precedes(0u, 130u) == false);
    BOOST_TEST(Traits::precedes(0u, 131u) == false);
    BOOST_TEST(Traits::precedes(0u, 132u) == false);

    BOOST_TEST(Traits::precedes(0u, 254u) == false);
    BOOST_TEST(Traits::precedes(0u, 255u) == false);

    BOOST_TEST(Traits::precedes(255u, 0u) == true);
    BOOST_TEST(Traits::precedes(254u, 0u) == true);
    BOOST_TEST(Traits::precedes(130u, 0u) == true);
    BOOST_TEST(Traits::precedes(129u, 0u) == true);
    BOOST_TEST(Traits::precedes(128u, 0u) == true);

    BOOST_TEST(Traits::precedes(127u, 0u) == false);
    BOOST_TEST(Traits::precedes(126u, 0u) == false);
    BOOST_TEST(Traits::precedes(125u, 0u) == false);
    BOOST_TEST(Traits::precedes(124u, 0u) == false);

    BOOST_TEST(Traits::precedes(255u, 0u) == true);
    BOOST_TEST(Traits::precedes(255u, 1u) == true);
    BOOST_TEST(Traits::precedes(255u, 2u) == true);
    BOOST_TEST(Traits::precedes(255u, 126u) == true);
    BOOST_TEST(Traits::precedes(255u, 127u) == true);
    BOOST_TEST(Traits::precedes(255u, 128u) == false);
    BOOST_TEST(Traits::precedes(255u, 129u) == false);
    BOOST_TEST(Traits::precedes(255u, 130u) == false);
    BOOST_TEST(Traits::precedes(255u, 254u) == false);
}

BOOST_AUTO_TEST_CASE(SequenceTraits_unsigned)
{
    using Traits = SequenceTraits<unsigned>;

    BOOST_TEST(Traits::precedes(Traits::initial_sequence, std::numeric_limits<Traits::value_type>::max()) == false);

    BOOST_TEST(Traits::precedes(Traits::initial_sequence, 0u) == true);
    BOOST_TEST(Traits::precedes(0u, std::numeric_limits<Traits::value_type>::max() - 1) == false);
    BOOST_TEST(Traits::difference(0u, std::numeric_limits<Traits::value_type>::max() - 1) == 2);

    /*
    BOOST_TEST(Traits::difference());

    BOOST_TEST(Traits::precedes(std::numeric_limits<Traits::value_type>::max() - 1, 0u) == false);
    BOOST_TEST(Traits::precedes(std::numeric_limits<Traits::value_type>::max(), 0u) == false);
*/

    BOOST_TEST(Traits::initial_sequence == std::numeric_limits<Traits::value_type>::max());

    BOOST_TEST(Traits::precedes(std::numeric_limits<Traits::value_type>::max() / 2 - 1, 0u) == false);
    BOOST_TEST(Traits::precedes(std::numeric_limits<Traits::value_type>::max() / 2, 0u) == false);

    BOOST_TEST(Traits::precedes(std::numeric_limits<Traits::value_type>::max() - 1, Traits::initial_sequence));
    BOOST_TEST(!Traits::precedes(std::numeric_limits<Traits::value_type>::max(), Traits::initial_sequence));
    // Тесты границы сверху
}

// Параметризировать тест типом
