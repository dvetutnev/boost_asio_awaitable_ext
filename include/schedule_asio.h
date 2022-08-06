#pragma once

#include <boost/asio/post.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/use_awaitable.hpp>

inline auto schedule(boost::asio::any_io_executor executor)
{
    auto initiate = [executor]<typename Handler>(Handler&& handler) mutable
    {
        boost::asio::post(executor, [handler = std::forward<Handler>(handler)]() mutable
        {
            handler();
        });
    };

    return boost::asio::async_initiate<
            decltype(boost::asio::use_awaitable), void()>(
                initiate, boost::asio::use_awaitable);
}
