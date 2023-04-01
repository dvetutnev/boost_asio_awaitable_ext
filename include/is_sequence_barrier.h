#pragma once

#include <boost/asio/awaitable.hpp>
#include <concepts>

namespace boost::asio::awaitable_ext {

template<typename Barrier, typename TSequence>
concept IsSequenceBarrier = requires(Barrier b, TSequence s) {
    { b.wait_until_published(s) } -> std::same_as<awaitable<TSequence>>;
};

} // namespace boost::asio::awaitable_ext
