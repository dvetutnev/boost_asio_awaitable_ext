#pragma once

#include "message.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/coro.hpp>

#include <memory>
#include <functional>

namespace nats_coro {

using namespace boost::asio;
using experimental::coro;

struct Unsub
{
    awaitable<void> operator()();

    struct Impl;
    std::shared_ptr<Impl> _impl;
};

class IClient
{
public:
    virtual awaitable<void> run() = 0;

    virtual awaitable<void> publish(std::string_view subject,
                                    std::string_view payload) = 0;

    using Subscribe = std::tuple<coro<Message>, Unsub>;
    virtual awaitable<Subscribe> subscribe(std::string_view subject) = 0;

    inline static const auto eof = Message();

    virtual ~IClient() = default;
};

awaitable<std::shared_ptr<IClient>> createClient(std::string_view url);

} // namespace nats_coro
