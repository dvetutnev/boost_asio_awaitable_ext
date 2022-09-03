///////////////////////////////////////////////////////////////////////////////
// Origin: https://github.com/lewissbaker/cppcoro/blob/master/include/cppcoro/sequence_traits.hpp
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <type_traits>

template<typename Sequence>
struct SequenceTraits
{
    using value_type = Sequence;
    using difference_type = std::make_signed_t<Sequence>;
    using size_type = std::make_unsigned_t<Sequence>;

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
