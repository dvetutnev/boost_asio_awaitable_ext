#pragma once

#include <boost/asio/awaitable.hpp>
#include <memory>

namespace nats_coro {

using namespace boost::asio;

class IClient
{
public:
    virtual awaitable<void> run() = 0;

    virtual awaitable<void> publish(std::string_view subject,
                                    std::string_view payload) = 0;

    virtual ~IClient() = default;
};

awaitable<std::shared_ptr<IClient>> createClient(std::string_view url);

} // namespace nats_coro
