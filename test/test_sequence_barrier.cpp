#include "sequence_barrier.h"
#include "schedule.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

namespace boost::asio::awaitable_ext::test {

using SequenceTypes = boost::mpl::list<std::uint8_t,
                                       std::uint16_t,
                                       std::uint32_t,
                                       std::size_t>;

BOOST_AUTO_TEST_CASE_TEMPLATE(SequenceTraits_pre, T, SequenceTypes)
{
    SequenceBarrier<T> sequenceBarrier;

    auto check = [&]() -> awaitable<void> {
        {
            auto lastUntilPublish = co_await sequenceBarrier.wait_until_publish(std::numeric_limits<T>::max() / 2 + 1);
            BOOST_TEST(lastUntilPublish == std::numeric_limits<T>::max());
        }
        {
            auto lastUntilPublish = co_await sequenceBarrier.wait_until_publish(std::numeric_limits<T>::max() / 2 + 43);
            BOOST_TEST(lastUntilPublish == std::numeric_limits<T>::max());
        }
        {
            auto lastUntilPublish = co_await sequenceBarrier.wait_until_publish(std::numeric_limits<T>::max());
            BOOST_TEST(lastUntilPublish == std::numeric_limits<T>::max());
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
