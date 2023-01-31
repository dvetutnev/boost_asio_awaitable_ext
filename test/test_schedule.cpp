#include "schedule.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/thread_pool.hpp>

#include <boost/test/unit_test.hpp>

#include <map>
#include <thread>

namespace boost::asio::awaitable_ext::test {

BOOST_AUTO_TEST_SUITE(tests_schedule);

BOOST_AUTO_TEST_CASE(_)
{
    bool reachedPointA = false;
    bool reachedPointB = false;

    auto process = [&]() -> awaitable<void> {
            reachedPointA = true;

            any_io_executor executor = co_await this_coro::executor;
            co_await schedule(executor);

            reachedPointB = true;
            co_return;
    };

    auto check = [&]() -> awaitable<void> {
            BOOST_TEST(reachedPointA);
            BOOST_TEST(!reachedPointB);
            co_return;
    };

    auto main = [&]() -> awaitable<void> {
            using namespace experimental::awaitable_operators;
            co_await(process() && check());
            co_return;
    };

    io_context ioContext;
    co_spawn(ioContext, main(), detached);
    ioContext.run();

    BOOST_TEST(reachedPointA);
    BOOST_TEST(reachedPointB);
}

BOOST_AUTO_TEST_CASE(multithread)
{
    thread_pool pool;
    std::map<std::thread::id, std::size_t> ids;

    auto main = [&]() -> awaitable<void> {
        for (std::size_t i = 0; i < 1'000; i++) {
             co_await schedule(pool.get_executor());
             ++ids[std::this_thread::get_id()];
        }
        co_return;
    };

    co_spawn(pool.get_executor(), main(), detached);
    pool.join();

    std::size_t sum = 0;
    for (const auto& [id, count] : ids) {
        sum += count;
        std::cout << id << ' ' << count << std::endl;
    }

    BOOST_TEST(sum == 1'000);
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace boost::asio::awaitable_ext::test
