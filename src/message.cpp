#include "message.h"

#include <boost/system.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>

namespace nats_coro {

// MSG FOO.BAR 42 11␍␊Hello World␍␊
// MSG FOO.BAR 42 GREETING.34 11␍␊Hello World␍␊
Message Message::parse(std::string&& data)
{
    Message::Fields fields;

    auto begin = std::begin(data);
    auto prev = begin;
    auto it = begin;
    const auto end = std::end(data);

    auto is_space = [](auto c) -> bool { return std::isspace(c); };

    for (Field field : std::to_array({Field::op_name, Field::subject, Field::subscribe_id}))
    {
        it = std::find_if(prev, end, is_space);
        if (it != end) {
            fields[std::to_underlying(field)] = {std::distance(begin, prev), std::distance(prev, it)};
            prev = std::find_if_not(it, end, is_space);
        }
    }

    it = std::find_if(prev, end, is_space);
    if (it != end && *it != '\r') {
        fields[std::to_underlying(Field::reply_to)] = {std::distance(begin, prev), std::distance(prev, it)};
        prev = std::find_if_not(it, end, is_space);
        it = std::find_if(prev, end, is_space);
    }

    std::size_t payloadSize = 0;
    auto [ptr, ec] = std::from_chars(prev.base(), it.base(), payloadSize);
    if (ec != std::errc()) {
        throw boost::system::system_error{std::make_error_code(ec), "Can`t parse payload size"};
    } else if (ptr != it.base()) {
        throw boost::system::system_error{std::make_error_code(std::errc::invalid_argument), "Can`t parse payload size"};
    }

    std::size_t payloadPos = std::distance(begin, it + 2);
    if (payloadPos + payloadSize + 2 > data.size()) {
        throw boost::system::system_error{std::make_error_code(std::errc::invalid_argument), "Payload not complete"};
    } else if (payloadPos + payloadSize + 2 < data.size()) {
        throw boost::system::system_error{std::make_error_code(std::errc::invalid_argument), "Payload to long"};
    }
    fields[std::to_underlying(Field::payload)] = {payloadPos, payloadSize};

    return {std::move(data), fields};
}

} // namespace nats_coro
