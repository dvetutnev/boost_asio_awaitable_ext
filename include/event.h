#pragma once

#include "boost/asio/use_awaitable.hpp"

struct Event
{
    boost::asio::awaitable<void> wait(boost::asio::any_io_executor executor) {
        auto initiate = [this, executor]<typename Handler>(Handler&& handler) mutable
        {
            this->_handler = [executor, handler = std::forward<Handler>(handler)]() mutable {
                boost::asio::post(executor, std::move(handler));
            };

            State oldState = State::not_set;
            const bool isWaiting = _state.compare_exchange_strong(
                oldState,
                State::not_set_consumer_waiting,
                std::memory_order_release,
                std::memory_order_acquire);

            if (!isWaiting) {
                this->_handler();
            }
        };

        return boost::asio::async_initiate<
            decltype(boost::asio::use_awaitable), void()>(
                initiate, boost::asio::use_awaitable);
    }

    void set() {
        const State oldState = _state.exchange(State::set, std::memory_order_acq_rel);

        if (oldState == State::not_set_consumer_waiting) {
            _handler();
        }
    }

    enum class State { not_set, not_set_consumer_waiting, set };
    std::atomic<State> _state = State::not_set;
    std::move_only_function<void()> _handler;
};
