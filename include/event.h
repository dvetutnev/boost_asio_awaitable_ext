#pragma once

#include <boost/asio/use_awaitable.hpp>

namespace boost::asio::awaitable_ext {

class Event
{
    enum class State { not_set, not_set_consumer_waiting, set };
    mutable std::atomic<State> _state;
    mutable std::move_only_function<void()> _handler;

public:
    Event() : _state{State::not_set} {}

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    [[nodiscard]] awaitable<void> wait(any_io_executor executor) const {
        auto initiate = [this, executor]<typename Handler>(Handler&& handler) mutable
        {
            this->_handler = [executor, handler = std::forward<Handler>(handler)]() mutable {
                post(executor, std::move(handler));
            };

            State oldState = State::not_set;
            const bool isWaiting = _state.compare_exchange_strong(
                oldState,
                State::not_set_consumer_waiting,
                std::memory_order_release,
                std::memory_order_relaxed);

            if (!isWaiting) {
                this->_handler();
            }
        };

        return async_initiate<
            decltype(use_awaitable), void()>(
                initiate, use_awaitable);
    }

    void set() {
        const State oldState = _state.exchange(State::set,
                                               std::memory_order_acquire);
        if (oldState == State::not_set_consumer_waiting) {
            _handler();
        }
    }
};

} // namespace boost::asio::awaitable_ext
