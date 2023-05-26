#pragma once

#include "sequence_barrier.h"
#include "multi_producer_sequencer.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace nats_coro {

using namespace boost::asio;

auto connect_to_nats(std::string_view host,
                     std::string_view port,
                     std::string_view token) -> awaitable<ip::tcp::socket>;

class TXQueue
{
public:
    explicit TXQueue(std::size_t bufferSize)
        :
        _buffer{std::make_unique<std::string[]>(bufferSize)},
        _readBarrier{},
        _sequencer{_readBarrier, bufferSize}
    {}

    awaitable<void> push(std::string&& item) {
        std::size_t seq = co_await _sequencer.claim_one();
        _buffer[seq & _sequencer.index_mask()] = std::move(item);
        _sequencer.publish(seq);
    }

    awaitable<awaitable_ext::SequenceRange<std::size_t>> get() {
        std::size_t awailable = co_await _sequencer.wait_until_published(_nextToRead, _nextToRead - 1);
        auto begin = _nextToRead;
        auto end = static_cast<std::size_t>(awailable + 1);
        auto result = awaitable_ext::SequenceRange<std::size_t>(begin, end);
        _nextToRead = end;
        co_return result;
    }

    std::string& operator[](std::size_t seq) {
        return _buffer[seq & _sequencer.index_mask()];
    }

    void consume(awaitable_ext::SequenceRange<std::size_t> range) {
        _readBarrier.publish(range.back());
    }

private:
    std::unique_ptr<std::string[]> _buffer;
    awaitable_ext::SequenceBarrier<std::size_t> _readBarrier;
    awaitable_ext::MultiProducerSequencer<std::size_t> _sequencer;

    std::size_t _nextToRead = 0;
};

class Client
{
public:
    Client(ip::tcp::socket&& socket, std::size_t txBufferSize = 64) : _socket{std::move(socket)}, _txQueue{txBufferSize} {}
    awaitable<void> run();

    awaitable<void> publish(std::string_view subject,
                            std::string_view payload);

private:
    ip::tcp::socket _socket;
    TXQueue _txQueue;

    awaitable<void> rx();
    awaitable<void> tx();

    awaitable<std::string> get_message();
};

} // namespace nats_coro
