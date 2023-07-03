#include "queue.h"
#include "utils.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/co_spawn.hpp>

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <optional>
#include <type_traits>

namespace nats_coro::test {

BOOST_AUTO_TEST_SUITE(tests_Queue);

namespace {
auto test_transfer(auto queues)
{
    auto [head, tail] = std::move(queues);

    auto producer = [&](std::uint64_t iterationCount) -> awaitable<void>
    {
        for (std::size_t i = 1; i <= iterationCount; i++) {
            co_await tail.push(std::size_t{i});
        }
        co_await tail.push(std::size_t{0});
    };

    auto consumer = [&](std::uint64_t& result) -> awaitable<void>
    {
        bool reachedEnd = false;
        do {
            auto range = co_await head.get();
            for (std::size_t seq : range) {
                result += head[seq];
                reachedEnd = head[seq] == 0;
            }
            head.consume(range);

        } while (!reachedEnd);
    };

    constexpr std::size_t iterationCount = 100'000;
    std::uint64_t result = 0;

    thread_pool tp{2};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    co_spawn(executorA, producer(iterationCount), rethrow_handler);
    co_spawn(executorB, consumer(result), rethrow_handler);
    tp.join();

    constexpr std::uint64_t expectedResult =
        static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result == expectedResult);
}
} // Anonymous namespace

BOOST_AUTO_TEST_CASE(transfer_mp) { test_transfer(make_queue_mp<std::size_t>(64)); }
BOOST_AUTO_TEST_CASE(transfer_sp) { test_transfer(make_queue_sp<std::size_t>(64)); }

namespace {
auto test_push_after_close(auto& queueTail) -> awaitable<void>
{
    try {
        co_await queueTail.push(std::size_t{42});
        BOOST_FAIL("Exception not throwing");
    } catch (const boost::system::system_error& ex) {
        BOOST_TEST(ex.code() == error::operation_aborted);
    }
}
} // Anonymous namespace

BOOST_AUTO_TEST_CASE(head_close)
{
    auto [head, tail] = make_queue_sp<std::size_t>(64);
    head.close();

    auto ioContext = io_context();
    co_spawn(ioContext, test_push_after_close(tail), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(head_close_dtor)
{
    auto [head, tail] = make_queue_sp<std::size_t>(64);
    auto wrap = std::make_optional(std::move(head));
    wrap.reset();

    auto ioContext = io_context();
    co_spawn(ioContext, test_push_after_close(tail), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace nats_coro::test
