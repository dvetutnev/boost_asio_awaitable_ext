#include "sequence_barrier.h"
#include "schedule.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/test/unit_test.hpp>

namespace boost::asio::awaitable_ext::test {

BOOST_AUTO_TEST_CASE(SequenceTraits_pre)
{
    SequenceBarrier<unsigned char> sequenceBarrier;

    auto check = [&]() -> awaitable<void> {
        {
            auto lastUntilPublish = co_await sequenceBarrier.wait_until_publish(128u);
            BOOST_TEST(lastUntilPublish == 255u);
        }
        {
            auto lastUntilPublish = co_await sequenceBarrier.wait_until_publish(128u);
            BOOST_TEST(lastUntilPublish == 255u);
        }
        {
            auto lastUntilPublish = co_await sequenceBarrier.wait_until_publish(255u);
            BOOST_TEST(lastUntilPublish == 255u);
        }
        co_return;

    };
    auto main = [&]() -> awaitable<void> {
        using namespace experimental::awaitable_operators;
        co_await(check());
        co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, main(), detached);
    ioContext.run();


}

} // namespace boost::asio::awaitable_ext::test
