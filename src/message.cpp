#include "message.h"

#include <boost/system.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>

namespace nats_coro {

template<std::size_t maxTokens>
auto split_control_line(std::string_view data)
{
    std::array<std::pair<std::size_t, std::size_t>, maxTokens> fields;
    std::size_t count = 0;

    auto begin = std::begin(data);
    auto prev = begin;
    const auto end = std::end(data);

    auto is_space = [](auto c) -> bool { return std::isspace(c); };

    for (std::size_t i = 0; i < maxTokens; i++)
    {
        auto it = std::find_if(prev, end, is_space);
        if (it == end) {
            break;
        }

        fields[i] = {std::distance(begin, prev),
                     std::distance(prev, it)};
        count++;
        if (*it == '\r') {
            assert(std::next(it) != end);
            assert(* std::next(it) == '\n');
            break;
        }

        prev = std::find_if_not(it, end, is_space);
    }

    return std::make_tuple(fields, count);
}

std::size_t parse_size(std::string_view str)
{
    std::size_t payloadSize;

    const char* begin = str.data();
    const char* end = begin + str.size();

    auto [ptr, ec] = std::from_chars(begin,
                                     end,
                                     payloadSize);
    if (ec != std::errc()) {
        throw boost::system::system_error{std::make_error_code(ec), __func__};
    } else if (ptr != end) {
        throw boost::system::system_error{std::make_error_code(std::errc::invalid_argument), __func__};
    }

    return payloadSize;
}

// MSG FOO.BAR 42 11␍␊Hello World␍␊
// MSG FOO.BAR 42 GREETING.34 11␍␊Hello World␍␊
ControlLineView parse_msg(std::string_view data)
{
    assert(data.starts_with("MSG"));
    constexpr std::size_t maxTokens = std::to_underlying(ControlLineView::Field::_size);

    auto [fields, count] = split_control_line<maxTokens>(data);
    if (count < maxTokens) {
        std::swap(fields[std::to_underlying(ControlLineView::Field::payload_size)],
                  fields[std::to_underlying(ControlLineView::Field::reply_to)]);
    }

    auto [offset, size] = fields[std::to_underlying(ControlLineView::Field::payload_size)];
    std::size_t payloadSize = parse_size(data.substr(offset, size));

    return {fields, data, payloadSize};
}

Message make_message(std::string&& data) {
    ControlLineView head = parse_msg(data);

    auto payload = std::pair<std::size_t, std::size_t>{};
    if (head.payload_size()) {
        auto pos = data.find("\r\n");
        if (pos != std::string::npos) {
            if (auto payloadOffset = pos + 2; payloadOffset < data.size()) {
                payload.first = payloadOffset;
                payload.second = std::min(head.payload_size(),
                                          data.size() - payloadOffset);
            }
        }
    }
    assert(head.payload_size() == payload.second);

    return {std::move(data), head, payload};
}

} // namespace nats_coro
