#include <boost/asio/system_timer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

namespace {
boost::asio::awaitable<void> timer(std::chrono::milliseconds duration) {
    auto executor = co_await boost::asio::this_coro::executor;
    boost::asio::system_timer timer{executor};
    timer.expires_after(duration);

    co_await timer.async_wait(boost::asio::use_awaitable);
};
} // Anonymous namesapce

TEST(asio_deadline_timer, _) {
    boost::asio::io_context ioContext;

    auto start = std::chrono::system_clock::now();

    boost::asio::co_spawn(ioContext, timer(150ms), boost::asio::detached);
    ioContext.run();

    auto duration = std::chrono::system_clock::now() - start;

    EXPECT_TRUE(duration >= 100ms);
    EXPECT_TRUE(duration <= 200ms);
}
