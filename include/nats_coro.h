#pragma once

#include "queue.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <optional>

namespace nats_coro {

using namespace boost::asio;

auto connect_to_nats(std::string_view host,
                     std::string_view port,
                     std::string_view token) -> awaitable<ip::tcp::socket>;
class Client
{
public:
    Client(ip::tcp::socket&& socket, std::size_t txBufferSize = 64) : _socket{std::move(socket)} {}
    awaitable<void> run();

    awaitable<void> publish(std::string_view subject,
                            std::string_view payload);

private:
    ip::tcp::socket _socket;

    using TXQueueFront = std::decay_t<decltype(std::get<0>(make_queue_mp<std::string>(0)))>;
    using TXQueueBack = std::decay_t<decltype(std::get<1>(make_queue_mp<std::string>(0)))>;

    std::optional<TXQueueFront> _txQueueFront;

    awaitable<void> rx();
    awaitable<void> tx(TXQueueBack&&);

    awaitable<std::string> get_message();
};

} // namespace nats_coro
