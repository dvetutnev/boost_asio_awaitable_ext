#include "connect_to_nats.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/json/serialize.hpp>
#include <boost/json/parse.hpp>

#include <boost/url.hpp>

#include <format>

namespace nats_coro {

namespace json = boost::json;

auto connect_to_nats(std::string_view url) -> awaitable<ip::tcp::socket>
{
    boost::url_view r = * boost::urls::parse_uri(url);
    co_return co_await connect_to_nats(
        r.host(),
        r.port(),
        r.userinfo());
}

json::value build_handshake(const json::value& srvConfig, std::string_view token) {
    return {
        {"verbose", false},
        {"pedantic", true},
        {"tls_required", false},
        {"auth_token", token},
        {"lang", "C++23"},
        {"version", "0.1"}
    };
}

auto connect_to_nats(std::string_view host,
                     std::string_view port,
                     std::string_view token) -> awaitable<ip::tcp::socket>
{
    auto executor = co_await this_coro::executor;
    auto resolver = ip::tcp::resolver(executor);
    auto endpoints = co_await resolver.async_resolve(host,
                                                     port,
                                                     use_awaitable);
    auto socket = ip::tcp::socket(executor);
    co_await async_connect(socket,
                           endpoints,
                           use_awaitable);

    std::string buf;
    co_await async_read_until(socket,
                              dynamic_buffer(buf),
                              "\r\n",
                              use_awaitable);
    auto data = std::string_view(buf);

    auto head = std::string_view("INFO ");
    data.remove_prefix(head.size());
    json::value srvInfo = json::parse(data);

    std::string reply = std::format("CONNECT {}\r\n",
                                    json::serialize(build_handshake(srvInfo,
                                                                    token)));
    co_await async_write(socket,
                         buffer(reply),
                         use_awaitable);

    co_return socket;
}

} // namespace nats_coro
