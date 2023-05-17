#include "event.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/thread_pool.hpp>

#include <boost/test/unit_test.hpp>

namespace boost::asio::awaitable_ext::test {

namespace {
void simple_test(std::size_t count) {
    thread_pool tp{2};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();

    for (std::size_t i = 0; i < count; i++)
    {
        Event event;

        auto consumer = [&]() -> awaitable<void> {
            co_await event.wait(use_awaitable);
            co_return;
        };

        auto producer = [&]() -> awaitable<void> {
            event.set();
            co_return;
        };

        std::atomic_bool consumerDone{false};
        std::atomic_bool producerDone{false};

        co_spawn((i % 2) ? executorA : executorB,
                 consumer(),
                 [&](std::exception_ptr ex)
                 {
                     if (ex) std::rethrow_exception(ex);
                     consumerDone.store(true, std::memory_order_release);
                 });
        co_spawn((i % 2) ? executorB : executorA,
                 producer(),
                 [&](std::exception_ptr ex)
                 {
                     if (ex) std::rethrow_exception(ex);
                     producerDone.store(true, std::memory_order_release);
                 });

        while (!consumerDone.load(std::memory_order_acquire) ||
               !producerDone.load(std::memory_order_acquire))
            ;
    };

    tp.join();
}
} // Anonymous namespace

BOOST_AUTO_TEST_CASE(Event_1) { simple_test(1); }
BOOST_AUTO_TEST_CASE(Event_10k) { simple_test(10'000); }

namespace {
void cancel_test(std::size_t count) {
    thread_pool tp{3};
    any_io_executor executorA = tp.get_executor();
    any_io_executor executorB = tp.get_executor();
    any_io_executor executorC = tp.get_executor();

    for (std::size_t i = 0; i < count; i++)
    {
        Event event;

        auto consumer = [&]() -> awaitable<void> {
            auto [ec] = co_await event.wait(as_tuple(use_awaitable));
            (void)ec;
            co_return;
        };

        auto producer = [&]() -> awaitable<void> {
            event.set();
            co_return;
        };

        auto cancel = [&]() -> awaitable<void> {
            event.cancel();
            co_return;
        };

        std::atomic_bool consumerDone{false}, producerDone{false}, cancelDone{false};

        auto handler = [](std::atomic_bool& flag)
        {
            return [&flag](std::exception_ptr ex)
            {
                if (ex) std::rethrow_exception(ex);
                flag.store(true, std::memory_order_release);
            };
        };

        co_spawn(executorA, consumer(), handler(consumerDone));
        co_spawn((i % 2) ? executorB : executorC, producer(), handler(producerDone));
        co_spawn((i % 2) ? executorC : executorB, cancel(), handler(cancelDone));

        while (!consumerDone.load(std::memory_order_acquire) ||
               !producerDone.load(std::memory_order_acquire) ||
               !cancelDone.load(std::memory_order_acquire))
            ;
    };

    tp.join();
}
} // Anonymous namespace

BOOST_AUTO_TEST_CASE(Event_cancel_1) { cancel_test(1); }
BOOST_AUTO_TEST_CASE(Event_cancel_10k) { cancel_test(10'000); }

} // namespace boost::asio::awaitable_ext::test
