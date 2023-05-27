#include "utils.h"

#include <boost/asio/system_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

boost::asio::awaitable<void> async_sleep(std::chrono::milliseconds duration) {
    auto timer = boost::asio::system_timer(co_await boost::asio::this_coro::executor);
    timer.expires_after(duration);
    co_await timer.async_wait(boost::asio::use_awaitable);
};

void rethrow_handler(std::exception_ptr ex) {
    if (!!ex) {
        std::rethrow_exception(ex);
    }
}
