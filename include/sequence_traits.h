///////////////////////////////////////////////////////////////////////////////
// Origin: https://github.com/lewissbaker/cppcoro/blob/master/include/cppcoro/sequence_traits.hpp
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <type_traits>

namespace boost::asio::awaitable_ext {

template<typename TSequence>
struct SequenceTraits
{
    using value_type = TSequence;
    using difference_type = std::make_signed_t<TSequence>;
    using size_type = std::make_unsigned_t<TSequence>;

    static constexpr value_type initial_sequence = static_cast<value_type>(-1);

    static constexpr difference_type difference(value_type a, value_type b)
    {
        return static_cast<difference_type>(a - b);
    }

    static constexpr bool precedes(value_type a, value_type b)
    {
        return difference(a, b) < 0;
    }
};

} // namespace boost::asio::awaitable_ext
