#include "single_producer_sequencer.h"
#include "async_sleep.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/test/unit_test.hpp>

namespace boost::asio::awaitable_ext::test {

using namespace experimental::awaitable_operators;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(tests_SingleProducerSequencer);

BOOST_AUTO_TEST_CASE(claim_one)
{
    constexpr std::size_t bufferSize = 256;
    constexpr std::size_t indexMask = bufferSize - 1;
    std::uint64_t buffer[bufferSize];

    constexpr std::size_t iterationCount = 1'000'000;

    auto producer = [&](SingleProducerSequencer<std::size_t>& sequencer) -> awaitable<void>
    {
        for (std::size_t i = 1; i <= iterationCount; i++)
        {
            auto seq = co_await sequencer.claim_one();
            buffer[seq & indexMask] = i;
            sequencer.publish(seq);
        }

        auto finalSeq = co_await sequencer.claim_one();
        buffer[finalSeq & indexMask] = 0;
        sequencer.publish(finalSeq);

        co_return;
    };

    std::uint64_t result = 0;

    auto consumer = [&](const SingleProducerSequencer<std::size_t>& sequencer,
                        SequenceBarrier<std::size_t>& consumerBarrier) -> awaitable<void>
    {
        bool reachedEnd = false;
        std::size_t nextToRead = 0;
        do {
            const std::size_t available = co_await sequencer.wait_until_published(nextToRead);
            do {
                result += buffer[nextToRead & indexMask];
            } while (nextToRead++ != available);

            // Zero value is sentinel that indicates the end of the stream.
            reachedEnd = buffer[available & indexMask] == 0;

            // Notify that we've finished processing up to 'available'.
            consumerBarrier.publish(available);

        } while (!reachedEnd);

        co_return;
    };

    SequenceBarrier<std::size_t> consumerBarrier;
    SingleProducerSequencer<std::size_t> sequencer{consumerBarrier, bufferSize};

    thread_pool tp{2};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    co_spawn(executorA, consumer(sequencer, consumerBarrier), detached);
    co_spawn(executorB, producer(sequencer), detached);
    tp.join();

    constexpr std::uint64_t expectedResult =
        static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result == expectedResult);
}

BOOST_AUTO_TEST_CASE(claim_up_to)
{
    constexpr std::size_t bufferSize = 256;
    constexpr std::size_t indexMask = bufferSize - 1;
    std::uint64_t buffer[bufferSize];

    constexpr std::size_t iterationCount = 1'000'000;

    auto producer = [&](SingleProducerSequencer<std::size_t>& sequencer) -> awaitable<void>
    {
        constexpr std::size_t maxBatchSize = 10;

        std::size_t i = 0;
        while (i < iterationCount)
        {
            std::size_t batchSize = std::min(maxBatchSize, iterationCount - i);
            auto range = co_await sequencer.claim_up_to(batchSize);
            for (auto seq : range) {
                buffer[seq & indexMask] = ++i;
            }
            sequencer.publish(range);
        }

        auto finalSeq = co_await sequencer.claim_one();
        buffer[finalSeq & indexMask] = 0;
        sequencer.publish(finalSeq);

        co_return;
    };

    std::uint64_t result = 0;

    auto consumer = [&](const SingleProducerSequencer<std::size_t>& sequencer,
                        SequenceBarrier<std::size_t>& consumerBarrier) -> awaitable<void>
    {
        bool reachedEnd = false;
        std::size_t nextToRead = 0;
        do {
            const std::size_t available = co_await sequencer.wait_until_published(nextToRead);
            do {
                result += buffer[nextToRead & indexMask];
            } while (nextToRead++ != available);

            // Zero value is sentinel that indicates the end of the stream.
            reachedEnd = buffer[available & indexMask] == 0;

            // Notify that we've finished processing up to 'available'.
            consumerBarrier.publish(available);

        } while (!reachedEnd);

        co_return;
    };

    SequenceBarrier<std::size_t> consumerBarrier;
    SingleProducerSequencer<std::size_t> sequencer{consumerBarrier, bufferSize};

    thread_pool tp{2};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    co_spawn(executorA, consumer(sequencer, consumerBarrier), detached);
    co_spawn(executorB, producer(sequencer), detached);
    tp.join();

    constexpr std::uint64_t expectedResult =
        static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result == expectedResult);
}

BOOST_AUTO_TEST_CASE(close)
{
    constexpr std::size_t bufferSize = 256;
    SequenceBarrier<std::size_t> readBarrier;
    SingleProducerSequencer<std::size_t> sequencer{readBarrier, bufferSize};
    int count = 0;

    auto producer = [&]() -> awaitable<void>
    {
        co_await sequencer.claim_up_to(bufferSize); // claim all buffer befor wait
        try {
            co_await sequencer.claim_one();
        } catch (const system::system_error& ex) {
            count++;
            BOOST_TEST(ex.code() == error::operation_aborted);
        }
    };

    auto consumer = [&]() -> awaitable<void>
    {
        try {
            co_await sequencer.wait_until_published(0);
        } catch (const system::system_error& ex) {
            count++;
            BOOST_TEST(ex.code() == error::operation_aborted);
        }
    };

    auto close = [&]() -> awaitable<void> { sequencer.close(); co_return; };
    auto handler = [](std::exception_ptr ex) { if (ex) std::rethrow_exception(ex); };

    io_context ioContext;
    co_spawn(ioContext, producer(), handler);
    co_spawn(ioContext, consumer(), handler);
    co_spawn(ioContext, close(), handler);
    ioContext.run();

    BOOST_TEST(count == 2);
}

BOOST_AUTO_TEST_SUITE(install_cancellation_handler);

BOOST_AUTO_TEST_CASE(claim_one)
{
    constexpr std::size_t bufferSize = 256;
    SequenceBarrier<std::size_t> readBarrier;
    SingleProducerSequencer<std::size_t> sequencer{readBarrier, bufferSize};

    auto producer = [&]() -> awaitable<void>
    {
        co_await sequencer.claim_up_to(bufferSize); // claim all buffer befor wait
        auto result = co_await(
            sequencer.claim_one() ||
            async_sleep(10ms)
            );
        BOOST_TEST(result.index() == 1); // timer win
    };

    bool consumerCanceled = false;
    auto consumer = [&]() -> awaitable<void>
    {
        try {
            co_await sequencer.wait_until_published(0);
        } catch (const system::system_error& ex) {
            consumerCanceled = true;
            BOOST_TEST(ex.code() == error::operation_aborted);
        }
    };

    auto handler = [](std::exception_ptr ex) { if (ex) std::rethrow_exception(ex); };

    io_context ioContext;
    co_spawn(ioContext, producer(), handler);
    co_spawn(ioContext, consumer(), handler);
    ioContext.run();

    BOOST_TEST(consumerCanceled);
}

BOOST_AUTO_TEST_CASE(claim_up_to)
{
    constexpr std::size_t bufferSize = 256;
    SequenceBarrier<std::size_t> readBarrier;
    SingleProducerSequencer<std::size_t> sequencer{readBarrier, bufferSize};

    auto producer = [&]() -> awaitable<void>
    {
        co_await sequencer.claim_up_to(bufferSize); // claim all buffer befor wait
        auto result = co_await(
            sequencer.claim_up_to(1) ||
            async_sleep(10ms)
            );
        BOOST_TEST(result.index() == 1); // timer win
    };

    bool consumerCanceled = false;
    auto consumer = [&]() -> awaitable<void>
    {
        try {
            co_await sequencer.wait_until_published(0);
        } catch (const system::system_error& ex) {
            consumerCanceled = true;
            BOOST_TEST(ex.code() == error::operation_aborted);
        }
    };

    auto handler = [](std::exception_ptr ex) { if (ex) std::rethrow_exception(ex); };

    io_context ioContext;
    co_spawn(ioContext, producer(), handler);
    co_spawn(ioContext, consumer(), handler);
    ioContext.run();

    BOOST_TEST(consumerCanceled);
}

BOOST_AUTO_TEST_CASE(wait_until_published)
{
    constexpr std::size_t bufferSize = 256;
    SequenceBarrier<std::size_t> readBarrier;
    SingleProducerSequencer<std::size_t> sequencer{readBarrier, bufferSize};

    bool producerCanceled = false;
    auto producer = [&]() -> awaitable<void>
    {
        co_await sequencer.claim_up_to(bufferSize); // claim all buffer befor wait
        try {
            co_await sequencer.claim_one();
        } catch (const system::system_error& ex) {
            producerCanceled = true;
            BOOST_TEST(ex.code() == error::operation_aborted);
        }
    };

    auto consumer = [&]() -> awaitable<void>
    {
        auto result = co_await(
            sequencer.wait_until_published(0) ||
            async_sleep(10ms)
            );
        BOOST_TEST(result.index() == 1); // timer win
    };

    auto handler = [](std::exception_ptr ex) { if (ex) std::rethrow_exception(ex); };

    io_context ioContext;
    co_spawn(ioContext, producer(), handler);
    co_spawn(ioContext, consumer(), handler);
    ioContext.run();

    BOOST_TEST(producerCanceled);
}

BOOST_AUTO_TEST_SUITE_END(); // install_cancellation_handler

BOOST_AUTO_TEST_SUITE_END(); // tests_SingleProducerSequencer

} // namespace boost::asio::awaitable_ext::test
