#include "sequence_barrier_mock_awaiter.h"

#include <boost/test/unit_test.hpp>

namespace boost::asio::awaitable_ext::test {

using AwaitersStorage = MockAwaitersStorage<std::uint8_t>;
using Awaiter = MockAwaiter<std::uint8_t>;

BOOST_AUTO_TEST_SUITE(tests_MockAwaitersStorage);

BOOST_AUTO_TEST_SUITE(get_upper);
BOOST_AUTO_TEST_CASE(next_free)
{
    AwaitersStorage storage;
    Awaiter* awaiter;

    awaiter = storage.get_upper(0);
    BOOST_REQUIRE(awaiter);
    BOOST_TEST(awaiter->targetSequence == 0);

    awaiter = storage.get_upper(0);
    BOOST_REQUIRE(awaiter);
    BOOST_TEST(awaiter->targetSequence == 1);

    awaiter = storage.get_upper(1);
    BOOST_REQUIRE(awaiter);
    BOOST_TEST(awaiter->targetSequence == 2);
}

BOOST_AUTO_TEST_CASE(first_free)
{
    AwaitersStorage storage;
    Awaiter* awaiter;

    awaiter = storage.get_upper(0);
    BOOST_REQUIRE(awaiter);
    BOOST_TEST(awaiter->targetSequence == 0);

    awaiter = storage.get_upper(42);
    BOOST_REQUIRE(awaiter);
    BOOST_TEST(awaiter->targetSequence == 42);

    awaiter = storage.get_upper(0);
    BOOST_REQUIRE(awaiter);
    BOOST_TEST(awaiter->targetSequence == 1);
}

BOOST_AUTO_TEST_CASE(no_free)
{
    AwaitersStorage storage;
    Awaiter* awaiter;

    for (std::uint8_t s = 1; s <= 129u; s++)
    {
        awaiter = storage.get_upper(1);
        BOOST_REQUIRE(awaiter);
        BOOST_TEST(awaiter->targetSequence == s);
    }

    awaiter = storage.get_upper(1);
    BOOST_TEST(awaiter == nullptr);
}
BOOST_AUTO_TEST_SUITE_END(); // get_upper

BOOST_AUTO_TEST_SUITE(get_lower);
BOOST_AUTO_TEST_CASE(next_free)
{
    AwaitersStorage storage;
    Awaiter* awaiter;

    awaiter = storage.get_lower(255u);
    BOOST_REQUIRE(awaiter);
    BOOST_TEST(awaiter->targetSequence == 255u);

    awaiter = storage.get_lower(255u);
    BOOST_REQUIRE(awaiter);
    BOOST_TEST(awaiter->targetSequence == 254u);

    awaiter = storage.get_lower(254u);
    BOOST_REQUIRE(awaiter);
    BOOST_TEST(awaiter->targetSequence == 253u);
}

BOOST_AUTO_TEST_CASE(first_free)
{
    AwaitersStorage storage;
    Awaiter* awaiter;

    awaiter = storage.get_lower(255u);
    BOOST_REQUIRE(awaiter);
    BOOST_TEST(awaiter->targetSequence == 255u);

    awaiter = storage.get_lower(242u);
    BOOST_REQUIRE(awaiter);
    BOOST_TEST(awaiter->targetSequence == 242u);

    awaiter = storage.get_lower(255u);
    BOOST_REQUIRE(awaiter);
    BOOST_TEST(awaiter->targetSequence == 254u);
}

BOOST_AUTO_TEST_CASE(no_free)
{
    AwaitersStorage storage;
    Awaiter* awaiter;

    for (std::uint8_t s = 255u; s >= 127u; s--)
    {
        awaiter = storage.get_lower(255u);
        BOOST_REQUIRE(awaiter);
        BOOST_TEST(awaiter->targetSequence == s);
    }

    awaiter = storage.get_lower(255u);
    BOOST_TEST(awaiter == nullptr);
}
BOOST_AUTO_TEST_SUITE_END(); // get_lower

BOOST_AUTO_TEST_SUITE_END();

} // namespace boost::asio::awaitable_ext::test
