#pragma once

#include <string>
#include <array>
#include <utility>
#include <cassert>

namespace nats_coro {

struct ControlLineView
{
public:
    std::string_view op_name() const { return get(Field::op_name); }
    std::string_view subscribe_id() const { return get(Field::subscribe_id); }
    std::string_view subject() const { return get(Field::subject); }
    std::string_view reply_to() const { return get(Field::reply_to); }
    std::size_t payload_size() const { return _payload_size; }

    ControlLineView() = default;

private:
    enum class Field : std::size_t
    {
        op_name = 0,
        subject,
        subscribe_id,
        reply_to,
        payload_size,

        _size
    };
    using Fields = std::array<std::pair<std::size_t, std::size_t>,
                              std::to_underlying(Field::_size)>;
    Fields _fields;
    std::string_view _data;
    std::size_t _payload_size = 0;

    std::string_view get(Field field) const {
        auto [offset, size] = _fields[std::to_underlying(field)];
        return {_data.data() + offset, size};
    }

    ControlLineView(Fields fields, std::string_view data, std::size_t payload_size) : _fields{fields}, _data{data}, _payload_size{payload_size} {}

    friend ControlLineView parse_msg(std::string_view);
    friend bool operator==(const ControlLineView& lhs, const ControlLineView& rhs);

    friend class Message;
    ControlLineView replace_storage(std::string_view data) { return {_fields, data, _payload_size}; }
};

inline bool operator==(const ControlLineView& lhs, const ControlLineView& rhs) {
    return lhs._fields == rhs._fields &&
           lhs._data == rhs._data &&
           lhs._payload_size == rhs._payload_size;
}

ControlLineView parse_msg(std::string_view);

class Message
{
public:
    ControlLineView head() const { return _head; }
    std::string_view payload() const { return {_data.data() + _payload.first,
                                               _payload.second}; }

    Message() = default;

    Message(std::string&& data,
             ControlLineView head,
             std::pair<std::size_t, std::size_t> payload)
        :
        _data{std::move(data)},
        _head{head.replace_storage(_data)},
        _payload{payload}
    {
        assert(_payload.first < _data.size());
        assert(_payload.first + _payload.second < _data.size());
    }

    Message(Message&& other)
        :
        _data{std::move(other._data)},
        _head{other._head.replace_storage(_data)},
        _payload{other._payload}
    {}

    Message& operator=(Message&& other) {
        _data = std::move(other._data);
        _head = other._head.replace_storage(_data);
        _payload = other._payload;
        return *this;
    }

private:
    std::string _data;
    ControlLineView _head;
    std::pair<std::size_t, std::size_t> _payload; // offset & size

    friend bool operator==(const Message&, const Message&);
};

inline bool operator==(const Message& lhs, const Message& rhs) {
    return lhs._data == rhs._data &&
           lhs._head == rhs._head &&
           lhs._payload == rhs._payload;
}

Message make_message(std::string&& data);

} // namespace nats_coro
