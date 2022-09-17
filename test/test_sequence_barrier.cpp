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

BOOST_AUTO_TEST_SUITE(tests_SequenceBarrier);
BOOST_AUTO_TEST_CASE_TEMPLATE(previous, T, SequenceTypes)
{
    SequenceBarrier<T> barrier;

    auto main = [&]() -> awaitable<void> {
        {
            auto lastUntilPublish = co_await barrier.wait_until_published(std::numeric_limits<T>::max() / 2 + 1);
            BOOST_TEST(lastUntilPublish == std::numeric_limits<T>::max());
        }
        {
            auto lastUntilPublish = co_await barrier.wait_until_published(std::numeric_limits<T>::max() / 2 + 43);
            BOOST_TEST(lastUntilPublish == std::numeric_limits<T>::max());
        }
        {
            auto lastUntilPublish = co_await barrier.wait_until_published(std::numeric_limits<T>::max());
            BOOST_TEST(lastUntilPublish == std::numeric_limits<T>::max());
        }
        co_return;

    };

    io_context ioContext;
    co_spawn(ioContext, main(), detached);
    ioContext.run();


}

BOOST_AUTO_TEST_CASE(wait_until_publish)
{
    SequenceBarrier<> barrier;
    bool reachedA = false;
    bool reachedB = false;
    bool reachedC = false;
    bool reachedD = false;
    bool reachedE = false;
    bool reachedF = false;

    auto consumer = [&]() -> awaitable<void> {
        BOOST_TEST(co_await barrier.wait_until_published(0) == 0);
        reachedA = true;
        BOOST_TEST(co_await barrier.wait_until_published(1) == 1);
        reachedB = true;
        BOOST_TEST(co_await barrier.wait_until_published(3) == 3);
        reachedC = true;
        BOOST_TEST(co_await barrier.wait_until_published(4) == 10);
        reachedD = true;
        co_await barrier.wait_until_published(5);
        reachedE = true;
        co_await barrier.wait_until_published(10);
        reachedF = true;
    };

    auto producer = [&]() -> awaitable<void> {
        BOOST_TEST(!reachedA);
        barrier.publish(0);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(reachedA);
        BOOST_TEST(!reachedB);
        barrier.publish(1);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(reachedB);
        BOOST_TEST(!reachedC);
        barrier.publish(2);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(!reachedC);
        barrier.publish(3);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(reachedC);
        BOOST_TEST(!reachedD);
        barrier.publish(10);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(reachedD);
        BOOST_TEST(reachedE);
        BOOST_TEST(reachedF);
    };

    auto main = [&]() -> awaitable<void> {
        using namespace experimental::awaitable_operators;
        co_await(consumer() && producer());
        co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, main(), detached);
    ioContext.run();
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace boost::asio::awaitable_ext::test
