#pragma once

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/system_timer.hpp>

namespace boost::asio::awaitable_ext::test {

inline awaitable<void> async_sleep(std::chrono::milliseconds duration) {
    system_timer timer{co_await this_coro::executor};
    timer.expires_after(duration);
    co_await timer.async_wait(use_awaitable);
};

}
