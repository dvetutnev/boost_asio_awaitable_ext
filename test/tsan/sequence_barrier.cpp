#include "sequence_barrier.h"
#include "sequence_barrier_mock_awaiter.h"

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/co_spawn.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

namespace boost::asio::awaitable_ext::test {

using SequenceTypes = boost::mpl::list<std::uint8_t,
                                       std::uint16_t>;

BOOST_AUTO_TEST_CASE_TEMPLATE(SequenceBarrier_, T, SequenceTypes)
{
    using BaseBarrier = SequenceBarrier<T, SequenceTraits<T>, MockAwaiter<T>>;
    struct Barrier : BaseBarrier
    {
        using BaseBarrier::add_awaiter;
    };
    Barrier barrier;

    std::atomic_bool producerDone = false;
    MockAwaitersStorage<T> awaiters;

    auto consumer = [&, previos = T{}] mutable
    {
        for (;;)
        {
            if (producerDone.load(std::memory_order_acquire)) {
                break;
            }
            const T lastPublished = barrier.last_published();
            if (lastPublished == previos) {
                continue;
            }
            previos = lastPublished;

            constexpr T quater = std::numeric_limits<T>::max() / 4;

            if (auto* awaiter = awaiters.get_upper(lastPublished + quater); awaiter != nullptr) {
                barrier.add_awaiter(awaiter);
            }
            if (auto* awaiter = awaiters.get_lower(lastPublished - quater); awaiter != nullptr) {
                barrier.add_awaiter(awaiter);
            }
        }
    };

    auto producer = [&]()
    {
        for (std::size_t i = 0; i <= std::numeric_limits<T>::max(); i++) {
            barrier.publish(i);
        }
        producerDone.store(true, std::memory_order_release);
        // Resume remaining awaiters
        for (std::size_t i = 0; i <= std::numeric_limits<T>::max(); i++) {
            barrier.publish(i);
        }
    };

    thread_pool tp{2};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    post(executorA, consumer);
    post(executorB, producer);
    tp.join();
}

namespace {
void cancel_test(std::size_t count) {
    thread_pool tp{4};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    any_io_executor executorC = tp.get_executor();
    any_io_executor executorD = tp.get_executor();

    for (std::size_t i = 0; i < count; i++)
    {
        SequenceBarrier<std::size_t> barrier;

        auto consumer = [&](std::size_t targetSequence) -> awaitable<void> {
            try {
                co_await barrier.wait_until_published(targetSequence);
            } catch (const system::system_error& ex) {
                assert(ex.code() == error::operation_aborted);
            }
        };

        auto producer = [&](std::size_t sequence) -> awaitable<void> {
            barrier.publish(sequence);
            co_return;
        };

        auto cancel = [&]() -> awaitable<void> {
            barrier.close();
            co_return;
        };

        std::atomic_bool consumerDone1{false}, consumerDone2{false}, producerDone{false}, cancelDone{false};

        auto handler = [](std::atomic_bool& flag)
        {
            return [&flag](std::exception_ptr ex)
            {
                if (ex) std::rethrow_exception(ex);
                flag.store(true, std::memory_order_release);
            };
        };

        co_spawn(executorA, consumer(27), handler(consumerDone1));
        co_spawn(executorB, consumer(68), handler(consumerDone2));
        co_spawn((i % 2) ? executorC : executorD, producer(42), handler(producerDone));
        co_spawn((i % 2) ? executorD : executorC, cancel(), handler(cancelDone));

        while (!consumerDone1.load(std::memory_order_acquire) ||
               !consumerDone2.load(std::memory_order_acquire) ||
               !producerDone.load(std::memory_order_acquire) ||
               !cancelDone.load(std::memory_order_acquire))
            ;
    };

    tp.join();
}
} // Anonymous namespace

BOOST_AUTO_TEST_CASE(SequenceBarrier_cancel_1) { cancel_test(1); }
BOOST_AUTO_TEST_CASE(SequenceBarrier_cancel_10k) { cancel_test(10'000); }

} // namespace boost::asio::awaitable_ext::test
