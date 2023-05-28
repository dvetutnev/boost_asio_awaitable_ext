#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace nats_coro {

using namespace boost::asio;

auto connect_to_nats(std::string_view url) -> awaitable<ip::tcp::socket>;
auto connect_to_nats(std::string_view host,
                     std::string_view port,
                     std::string_view token) -> awaitable<ip::tcp::socket>;

} // namespace nats_coro
