#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/thread_pool.hpp>

#include <boost/test/unit_test.hpp>

#include <map>
#include <mutex>
#include <thread>

namespace boost::asio::awaitable_ext::test {

BOOST_AUTO_TEST_CASE(asio_multithread)
{
    std::mutex mtx;
    std::map<std::thread::id, int> ids;

    const unsigned threadCount = std::thread::hardware_concurrency();
    thread_pool pool{threadCount};

    auto task = [&]() -> awaitable<void> {
        auto id = std::this_thread::get_id();
        std::lock_guard<std::mutex> lock{mtx};
        ++ids[id];
        co_return;
    };

    for (unsigned i = 0; i < threadCount; ++i) {
        co_spawn(pool.get_executor(), task(), detached);
    }

    pool.join();

    BOOST_TEST(ids.size() >= threadCount / 2);
}

} // namespace boost::asio::awaitable_ext::test
