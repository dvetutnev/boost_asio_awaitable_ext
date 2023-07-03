#pragma once

#include "sequence_barrier.h"
#include "is_sequencer.h"

#include "multi_producer_sequencer.h"
#include "single_producer_sequencer.h"

#include <memory>
#include <tuple>

namespace boost::asio::awaitable_ext {

extern template class SequenceTraits<std::size_t>;
extern template class SequenceRange<std::size_t>;
extern template class SequenceBarrier<std::size_t>;
extern template class MultiProducerSequencer<std::size_t>;
extern template class SingleProducerSequencer<std::size_t>;

} // namespace boost::asio::awaitable_ext

namespace nats_coro {

using namespace boost::asio;

template<typename T, awaitable_ext::IsSequencer<std::size_t> Sequencer>
inline auto make_queue(std::size_t bufferSize);

template<typename T, awaitable_ext::IsSequencer<std::size_t> Sequencer>
class QueueState
{
    struct Tag {};
    friend auto make_queue<T, Sequencer>(std::size_t);
public:
    explicit QueueState(std::size_t bufferSize, Tag)
        :
        _buffer{std::make_unique<T[]>(bufferSize)},
        _readBarrier{},
        _sequencer{_readBarrier, bufferSize},
        _nextToRead{0}
    {}

    T& operator[](std::size_t seq) { return _buffer[seq & _sequencer.index_mask()]; }

    // Front iface
    awaitable<std::size_t> claim_one() { return _sequencer.claim_one(); }
    void publish(std::size_t seq) { _sequencer.publish(seq); }

    // Back iface
    awaitable<awaitable_ext::SequenceRange<std::size_t>> get() {
        const std::size_t awailable = co_await wait(_nextToRead);
        const std::size_t begin = _nextToRead;
        const std::size_t end = static_cast<std::size_t>(awailable + 1);
        auto result = awaitable_ext::SequenceRange<std::size_t>(begin, end);
        _nextToRead = end;
        co_return result;
    }
    void consume(awaitable_ext::SequenceRange<std::size_t> range) { _readBarrier.publish(range.back()); }

    void close() { _sequencer.close(); }

private:
    std::unique_ptr<T[]> _buffer;
    awaitable_ext::SequenceBarrier<std::size_t> _readBarrier;
    Sequencer _sequencer;
    std::size_t _nextToRead;

    awaitable<std::size_t> wait(std::size_t nextToRead) {
        if constexpr (awaitable_ext::IsMultiProducer<Sequencer, std::size_t>) {
            return _sequencer.wait_until_published(nextToRead, nextToRead - 1);
        } else {
            return _sequencer.wait_until_published(nextToRead);
        }
    }
};

template<typename T, awaitable_ext::IsSequencer<std::size_t> Sequencer>
class QueueTail
{
    struct Tag {};
    friend auto make_queue<T, Sequencer>(std::size_t);
public:
    explicit QueueTail(std::shared_ptr<QueueState<T, Sequencer>> state, Tag) : _state{std::move(state)} {}
    QueueTail(QueueTail&) requires awaitable_ext::IsMultiProducer<Sequencer, std::size_t> = default;
    QueueTail& operator=(QueueTail&) requires awaitable_ext::IsMultiProducer<Sequencer, std::size_t> = default;
    QueueTail(QueueTail&&) = default;
    QueueTail& operator=(QueueTail&&) = default;

    awaitable<void> push(T&& item) {
        std::size_t seq = co_await _state->claim_one();
        (*_state)[seq] = std::move(item);
        _state->publish(seq);
    }

private:
    const std::shared_ptr<QueueState<T, Sequencer>> _state;
};

template<typename T, awaitable_ext::IsSequencer<std::size_t> Sequencer>
class QueueHead
{
    struct Tag {};
    friend auto make_queue<T, Sequencer>(std::size_t);
public:
    explicit QueueHead(std::shared_ptr<QueueState<T, Sequencer>> state, Tag) : _state{std::move(state)} {}
    QueueHead(const QueueHead&) = delete;
    QueueHead& operator=(const QueueHead&) = delete;
    QueueHead(QueueHead&& tmp) : _state{std::move(tmp._state)} { tmp._state.reset(); }
    QueueHead& operator=(QueueHead&& tmp) { _state = std::move(tmp._state); tmp._state.reset(); }
    ~QueueHead() { try { close(); } catch(...) {} }

    awaitable<awaitable_ext::SequenceRange<std::size_t>> get() { return _state->get(); }
    T& operator[](std::size_t seq) { return (*_state)[seq]; }
    void consume(awaitable_ext::SequenceRange<std::size_t> range) { _state->consume(range); }

    void close() { if (_state) { _state->close(); } }

private:
    std::shared_ptr<QueueState<T, Sequencer>> _state;
};

template<typename T, awaitable_ext::IsSequencer<std::size_t> Sequencer>
inline auto make_queue(std::size_t bufferSize)
{
    using State = QueueState<T, Sequencer>;
    using Head = QueueHead<T, Sequencer>;
    using Tail = QueueTail<T, Sequencer>;
    auto state = std::make_shared<State>(bufferSize, typename State::Tag{});
    return std::make_tuple(
        Head{state, typename Head::Tag{}},
        Tail{state, typename Tail::Tag{}}
        );
}

template<typename T>
inline auto make_queue_mp(std::size_t bufferSize) {
    return make_queue<T, awaitable_ext::MultiProducerSequencer<std::size_t>>(bufferSize);
}

template<typename T>
inline auto make_queue_sp(std::size_t bufferSize) {
    return make_queue<T, awaitable_ext::SingleProducerSequencer<std::size_t>>(bufferSize);
}

} // namespace nats_cor
