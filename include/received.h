#pragma once

#include <boost/asio/read_until.hpp>

namespace nats_coro {
struct received
{
    explicit received(std::size_t totalSize) : _totalSize{totalSize}, _received{0} {}

    auto operator()(auto begin, auto end)
    {
        _received += std::distance(begin, end);
        if (_received < _totalSize) {
            return std::make_pair(end, false);
        } else {
            auto it = begin + (_received - _totalSize);
            return std::make_pair(it, true);
        }
    };

    const std::size_t _totalSize;
    std::size_t _received;
};
}

namespace boost::asio {
template<>
struct is_match_condition<nats_coro::received>
{
    enum { value = true };
};
}
