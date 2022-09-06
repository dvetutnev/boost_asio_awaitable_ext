#pragma once

#include <boost/asio/use_awaitable.hpp>

namespace boost::asio::awaitable_ext {

inline auto schedule(any_io_executor executor) -> awaitable<void>
{
    auto initiate = [executor]<typename Handler>(Handler&& handler) mutable
    {
        post(executor, [handler = std::forward<Handler>(handler)]() mutable
        {
            handler();
        });
    };

    return async_initiate<
            decltype(use_awaitable), void()>(
                initiate, use_awaitable);
}

} // namespace boost::asio::awaitable_ext
