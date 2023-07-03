#pragma once

#include "message.h"
#include "queue.h"

#include <boost/asio/experimental/coro.hpp>

namespace nats_coro {

using boost::asio::any_io_executor;
using boost::asio::experimental::coro;

using SubQueueHead = std::decay_t<decltype(std::get<0>(make_queue_sp<Message>(0)))>;
using SubQueueTail = std::decay_t<decltype(std::get<1>(make_queue_sp<Message>(0)))>;

auto subscription(any_io_executor executor, SubQueueHead queue) -> coro<Message>;

} // namespace nats_coro
