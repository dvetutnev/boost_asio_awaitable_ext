#include "sequence_traits.h"

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <limits>
#include <cstdint>

namespace boost::asio::awaitable_ext::test {

using SequenceTypes = boost::mpl::list<std::uint8_t,
                                       std::uint16_t,
                                       std::uint32_t,
                                       std::size_t>;

BOOST_AUTO_TEST_SUITE(tests_SequenceTraits);

BOOST_AUTO_TEST_CASE_TEMPLATE(difference, T, SequenceTypes)
{
    using Traits = SequenceTraits<T>;
    using Value = typename Traits::value_type;
    using Difference = typename Traits::difference_type;

    BOOST_TEST(Traits::difference(0u, std::numeric_limits<Value>::max() / 2 - 1) == std::numeric_limits<Difference>::min() + 2);
    BOOST_TEST(Traits::difference(0u, std::numeric_limits<Value>::max() / 2) == std::numeric_limits<Difference>::min() + 1);
    BOOST_TEST(Traits::difference(0u, std::numeric_limits<Value>::max() / 2 + 1) == std::numeric_limits<Difference>::min());
    BOOST_TEST(Traits::difference(0u, std::numeric_limits<Value>::max() / 2 + 2) == std::numeric_limits<Difference>::max());
    BOOST_TEST(Traits::difference(0u, std::numeric_limits<Value>::max()) == 1);

    BOOST_TEST(Traits::difference(std::numeric_limits<Value>::max() / 2 - 1, 0) == std::numeric_limits<Difference>::max() - 1);
    BOOST_TEST(Traits::difference(std::numeric_limits<Value>::max() / 2, 0) == std::numeric_limits<Difference>::max());
    BOOST_TEST(Traits::difference(std::numeric_limits<Value>::max() / 2 + 1, 0) == std::numeric_limits<Difference>::min());
    BOOST_TEST(Traits::difference(std::numeric_limits<Value>::max() / 2 + 2, 0) == std::numeric_limits<Difference>::min() + 1);
    BOOST_TEST(Traits::difference(std::numeric_limits<Value>::max(), 0) == -1);
}

BOOST_AUTO_TEST_CASE(precedes_unsigned_char)
{
    using Traits = SequenceTraits<std::uint8_t>;

    BOOST_TEST(Traits::precedes(0u, 127u));
    BOOST_TEST(Traits::precedes(0u, 128u));
    BOOST_TEST(!Traits::precedes(0u, 129u));

    BOOST_TEST(Traits::precedes(128u, 255u));
    BOOST_TEST(Traits::precedes(128u, 0u));
    BOOST_TEST(!Traits::precedes(128u, 1u));

    BOOST_TEST(Traits::initial_sequence == 255u);

    BOOST_TEST(Traits::precedes(255u, 126u));
    BOOST_TEST(Traits::precedes(255u, 127u));
    BOOST_TEST(!Traits::precedes(255u, 128u));

    BOOST_TEST(Traits::precedes(127u, 254u));
    BOOST_TEST(Traits::precedes(127u, 255u));
    BOOST_TEST(!Traits::precedes(127u, 0u));
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace boost::asio::awaitable_ext::test
