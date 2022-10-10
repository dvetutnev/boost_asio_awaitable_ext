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
    };

    io_context ioContext;
    co_spawn(ioContext, main(), detached);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(single_consumer)
{
    SequenceBarrier<> barrier;
    bool reachedA = false;
    bool reachedB = false;
    bool reachedC = false;
    bool reachedD = false;
    bool reachedE = false;
    bool reachedF = false;

    auto consumer = [&]() -> awaitable<void> {
        std::cout << "start consumer" << std::endl;
        BOOST_TEST(co_await barrier.wait_until_published(0) == 0);
        reachedA = true;
        std::cout << "reachedA = true" << std::endl;
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
        std::cout << "start producer" << std::endl;
        BOOST_TEST(!reachedA);
        barrier.publish(0);
        std::cout << "barrier.publish(0)" << std::endl;
        co_await schedule(co_await this_coro::executor);
        std::cout << "resume producer" << std::endl;
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

BOOST_AUTO_TEST_CASE(multiply_consumers)
{
    SequenceBarrier<> barrier;
    bool reachedA = false;
    bool reachedB = false;
    bool reachedC = false;
    bool reachedD = false;
    bool reachedE = false;
    bool reachedF = false;
    bool reachedG = false;
    bool reachedH = false;

    auto consumer10 = [&]() -> awaitable<void> {
        BOOST_TEST(co_await barrier.wait_until_published(0) == 0);
        reachedA = true;
        BOOST_TEST(co_await barrier.wait_until_published(10) == 10);
        reachedB = true;
    };

    auto consumer17 = [&]() -> awaitable<void> {
        reachedC = true;
        BOOST_TEST(co_await barrier.wait_until_published(17) == 18);
        reachedD = true;
    };

    auto consumer18 = [&]() -> awaitable<void> {
        reachedE = true;
        BOOST_TEST(co_await barrier.wait_until_published(18) == 18);
        reachedF = true;
    };

    auto consumer20 = [&]() -> awaitable<void> {
        reachedG = true;
        BOOST_TEST(co_await barrier.wait_until_published(20) == 20);
        reachedH = true;
    };

    auto producer = [&]() -> awaitable<void> {
        BOOST_TEST(!reachedA);
        BOOST_TEST(!reachedB);
        BOOST_TEST(reachedC);
        BOOST_TEST(!reachedD);
        BOOST_TEST(reachedE);
        BOOST_TEST(!reachedF);
        BOOST_TEST(reachedG);
        BOOST_TEST(!reachedH);

        barrier.publish(0);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(reachedA);
        BOOST_TEST(!reachedB);
        BOOST_TEST(!reachedD);
        BOOST_TEST(!reachedF);
        BOOST_TEST(!reachedH);

        barrier.publish(10);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(reachedB);
        BOOST_TEST(!reachedD);
        BOOST_TEST(!reachedF);
        BOOST_TEST(!reachedH);

        barrier.publish(18);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(reachedD);
        BOOST_TEST(reachedF);
        BOOST_TEST(!reachedH);

        barrier.publish(20);
        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(reachedH);
    };

    auto main = [&]() -> awaitable<void> {
        using namespace experimental::awaitable_operators;
        co_await(consumer10() &&
                 consumer17() &&
                 consumer18() &&
                 consumer20() &&
                 producer());
        co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, main(), detached);
    ioContext.run();
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace boost::asio::awaitable_ext::test
