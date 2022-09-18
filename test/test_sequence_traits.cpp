#include "sequence_traits.h"

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <limits>

namespace boost::asio::awaitable_ext::test {

using SequenceTypes = boost::mpl::list<std::uint8_t,
                                       std::uint16_t,
                                       std::uint32_t,
                                       std::size_t>;

BOOST_AUTO_TEST_CASE_TEMPLATE(SequenceTraits_difference, T, SequenceTypes)
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

BOOST_AUTO_TEST_CASE(SequenceTraits_precedes_unsigned_char)
{
    using Traits = SequenceTraits<unsigned char>;

    assert(Traits::precedes(0u, 128u));
    assert(!Traits::precedes(0u, 129u));

    assert(Traits::initial_sequence == 255u);
    assert(Traits::precedes(255u, 0u));
    assert(Traits::precedes(255u, 127u));
    assert(!Traits::precedes(255u, 128u));
    assert(!Traits::precedes(255u, 255u));
}

} // namespace boost::asio::awaitable_ext::test
