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
    awaitable<void> operator()() {
        co_await std::move(*_push_unsub)();
    }

    ~Unsub() {}

    using Func = std::move_only_function<awaitable<void>()>;
    explicit Unsub(Func push_unsub) : _push_unsub{std::make_shared<Func>(std::move(push_unsub))} {}

    std::shared_ptr<Func> _push_unsub;
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
