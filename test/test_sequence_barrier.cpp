#include "sequence_barrier.h"
#include "schedule.h"
#include "utils.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/thread_pool.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

namespace boost::asio::awaitable_ext::test {

using SequenceTypes = boost::mpl::list<std::uint8_t,
                                       std::uint16_t,
                                       std::uint32_t,
                                       std::size_t>;
using namespace std::chrono_literals;
using namespace experimental::awaitable_operators;

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

BOOST_AUTO_TEST_CASE(previous_)
{
    using T = std::size_t;
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

        co_await schedule(co_await this_coro::executor);
        BOOST_TEST(reachedE);

        co_await schedule(co_await this_coro::executor);
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

BOOST_AUTO_TEST_CASE(multithread)
{
    SequenceBarrier<std::size_t> writeBarrier;
    SequenceBarrier<std::size_t> readBarrier;

    constexpr std::size_t iterationCount = 10'000'000;

    constexpr std::size_t bufferSize = 256;
    std::uint64_t buffer[bufferSize];
    std::uint64_t result = 0;

    auto consumer = [&]() -> awaitable<void>
    {
        bool reachedEnd = false;
        std::size_t nextToRead = 0;
        do {
            std::size_t available = co_await writeBarrier.wait_until_published(nextToRead);
            do {
                result += buffer[nextToRead % bufferSize];
            } while (nextToRead++ != available);

            // Zero value is sentinel that indicates the end of the stream.
            reachedEnd = buffer[available % bufferSize] == 0;

            // Notify that we've finished processing up to 'available'.
            readBarrier.publish(available);
        } while (!reachedEnd);
    };

    auto producer = [&]() -> awaitable<void>
    {
        std::size_t  available = readBarrier.last_published() + bufferSize;
        for (std::size_t nextToWrite = 0; nextToWrite <= iterationCount; ++nextToWrite)
        {
            if (SequenceTraits<std::size_t>::precedes(available, nextToWrite))
            {
                available = co_await readBarrier.wait_until_published(nextToWrite - bufferSize) + bufferSize;
            }

            if (nextToWrite == iterationCount)
            {
                // Write sentinel (zero) as last element.
                buffer[nextToWrite % bufferSize] = 0;
            }
            else
            {
                // Write value
                buffer[nextToWrite % bufferSize] = nextToWrite + 1;
            }

            // Notify consumer that we've published a new value.
            writeBarrier.publish(nextToWrite);
        }
    };

    thread_pool tp{2};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    co_spawn(executorA, consumer(), detached);
    co_spawn(executorB, producer(), detached);
    tp.join();

    constexpr std::uint64_t expectedResult =
        static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;

    BOOST_TEST(result == expectedResult);
}

BOOST_AUTO_TEST_CASE(close)
{
    SequenceBarrier<std::size_t> barrier;
    int count = 0;

    auto consumer = [&](std::size_t targetSequence) -> awaitable<void> {
        try {
            co_await barrier.wait_until_published(targetSequence);
            BOOST_FAIL("Exception not throwed");
        } catch (const boost::system::system_error& ex) {
            count++;
            BOOST_TEST(ex.code() == error::operation_aborted);
        }
    };

    auto close = [&]() -> awaitable<void> {
        barrier.close();
        co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, consumer(42), rethrow_handler);
    co_spawn(ioContext, consumer(89), rethrow_handler);
    co_spawn(ioContext, close(), rethrow_handler);
    ioContext.run();

    BOOST_TEST(count == 2);
}

BOOST_AUTO_TEST_CASE(install_cancellation_handler)
{
    auto main = [&]() -> awaitable<void>
    {
        SequenceBarrier<std::size_t> barrier;

        auto result = co_await(
            barrier.wait_until_published(11) ||
            barrier.wait_until_published(22) ||
            async_sleep(10ms)
            );

        BOOST_TEST(result.index() == 2); // timer win
    };

    io_context ioContext;
    co_spawn(ioContext, main(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace boost::asio::awaitable_ext::test
