#pragma once

#include <boost/asio/awaitable.hpp>

#include <chrono>
#include <exception>

boost::asio::awaitable<void> async_sleep(std::chrono::milliseconds duration);
void rethrow_handler(std::exception_ptr);
