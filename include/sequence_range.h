#pragma once

#include "sequence_traits.h"

#include <iterator>
#include <concepts>

namespace boost::asio::awaitable_ext {

template<std::unsigned_integral TSequence,
         typename Traits = SequenceTraits<TSequence>>
class SequenceRange
{
public:
    using value_type = TSequence;
    using difference_type = typename Traits::difference_type;
    using size_type = typename Traits::size_type;

    class const_iterator
    {
    public:

        using iterator_category = std::random_access_iterator_tag;
        using value_type = TSequence;
        using difference_type = typename Traits::difference_type;
        using reference = const TSequence&;
        using pointer = const TSequence*;

        explicit constexpr const_iterator(TSequence value) noexcept : _value(value) {}

        const TSequence& operator*() const noexcept { return _value; }
        const TSequence* operator->() const noexcept { return std::addressof(_value); }

        const_iterator& operator++() noexcept { ++_value; return *this; }
        const_iterator& operator--() noexcept { --_value; return *this; }

        const_iterator operator++(int) noexcept { return const_iterator(_value++); }
        const_iterator operator--(int) noexcept { return const_iterator(_value--); }

        constexpr difference_type operator-(const_iterator other) const noexcept { return Traits::difference(_value, other._value); }
        constexpr const_iterator operator-(difference_type delta) const noexcept { return const_iterator{ static_cast<TSequence>(_value - delta) }; }
        constexpr const_iterator operator+(difference_type delta) const noexcept { return const_iterator{ static_cast<TSequence>(_value + delta) }; }

        constexpr bool operator==(const_iterator other) const noexcept { return _value == other._value; }
        constexpr bool operator!=(const_iterator other) const noexcept { return _value != other._value; }

    private:
        TSequence _value;
    };

    constexpr SequenceRange() noexcept : _begin{}, _end{} {}
    constexpr SequenceRange(TSequence begin, TSequence end) noexcept : _begin{begin}, _end{end} {}

    constexpr const_iterator begin() const noexcept { return const_iterator(_begin); }
    constexpr const_iterator end() const noexcept { return const_iterator(_end); }

    constexpr TSequence front() const noexcept { return _begin; }
    constexpr TSequence back() const noexcept { return _end - 1; }

    constexpr size_type size() const noexcept
    {
        return static_cast<size_type>(Traits::difference(_end, _begin));
    }

    constexpr bool empty() const noexcept
    {
        return _begin == _end;
    }

    constexpr TSequence operator[](size_type index) const noexcept
    {
        return _begin + index;
    }

    constexpr SequenceRange first(size_type count) const noexcept
    {
        return SequenceRange{ _begin, static_cast<TSequence>(_begin + std::min(size(), count)) };
    }

    constexpr SequenceRange skip(size_type count) const noexcept
    {
        return SequenceRange{ _begin + std::min(size(), count), _end };
    }

private:
    TSequence _begin;
    TSequence _end;
};

} // namespace boost::asio::awaitable_ext
