#pragma once

#include <string>
#include <array>
#include <utility>

namespace nats_coro {

enum class Field : std::size_t
{
    op_name = 0,
    subject,
    subscribe_id,
    reply_to,
    payload,

    _size
};

struct Message
{
    std::string_view operator[](Field field) const {
        auto [offset, size] = _fields[std::to_underlying(field)];
        return {_data.data() + offset, size};
    }

    static Message parse(std::string&&);

    std::string _data;
    using Fields = std::array<std::pair<std::size_t, std::size_t>,
                              static_cast<std::size_t>(Field::_size)>;
    const Fields _fields;
};

} // namespace nats_coro
