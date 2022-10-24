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
    std::map<std::thread::id, int> cont;

    const unsigned threadCount = std::thread::hardware_concurrency();
    thread_pool threadPool{threadCount};

    auto task = [&]() -> awaitable<void> {
        auto id = std::this_thread::get_id();
        std::lock_guard<std::mutex> lock{mtx};
        ++cont[id];
        co_return;
    };

    for (unsigned i = 0; i < threadCount; ++i) {
        auto executor = threadPool.get_executor();
        co_spawn(executor, task(), detached);
    }

    threadPool.join();

    BOOST_TEST(cont.size() > threadCount / 2);
}

} // namespace boost::asio::awaitable_ext::test
