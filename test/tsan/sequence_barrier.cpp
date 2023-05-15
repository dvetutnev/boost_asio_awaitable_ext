#include "sequence_barrier.h"
#include "sequence_barrier_mock_awaiter.h"

#include <boost/asio/thread_pool.hpp>

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

} // namespace boost::asio::awaitable_ext::test
