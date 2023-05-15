#pragma once

#include <boost/asio/async_result.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/error.hpp>

namespace boost::asio::awaitable_ext {

class Event
{
    enum class State { not_set, not_set_consumer_waiting, set, canceled };
    mutable std::atomic<State> _state;
    mutable std::move_only_function<void(system::error_code)> _handler;

public:
    Event() : _state{State::not_set} {}

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    template<completion_token_for<void(system::error_code)> CompletionToken>
    auto wait(CompletionToken&& completionToken) const {
        auto initiate = [this](auto&& handler) mutable
        {
            auto slot = get_associated_cancellation_slot(handler, cancellation_slot());
            if (slot.is_connected()) {
                slot.assign([this](cancellation_type){ const_cast<Event*>(this)->cancel(); });
            }

            this->_handler = [executor = get_associated_executor(handler),
                              handler = std::move(handler)](system::error_code ec) mutable
            {
                auto wrap = [handler = std::move(handler), ec]() mutable
                {
                    handler(ec);
                };
                post(executor, std::move(wrap));
            };

            State oldState = State::not_set;
            const bool isWaiting = _state.compare_exchange_strong(
                oldState,
                State::not_set_consumer_waiting,
                std::memory_order_release,
                std::memory_order_relaxed);

            if (!isWaiting) {
                auto ec = (oldState == State::canceled) ? system::error_code{error::operation_aborted}
                                                        : system::error_code{}; // not error
                this->_handler(ec);
            }
        };

        return async_initiate<
            CompletionToken, void(system::error_code)>(
                initiate, completionToken);
    }

    void set() {
        State oldState = State::not_set;
        bool isSet = _state.compare_exchange_strong(
            oldState,
            State::set,
            std::memory_order_release,
            std::memory_order_acquire); // see change of handler if current state is not_set_consumer_waiting

        if (isSet) {
            return; // set before wait
        }
        else if (oldState == State::not_set_consumer_waiting) {
            // wait win
            isSet = _state.compare_exchange_strong(
                oldState,
                State::set,
                std::memory_order_relaxed,
                std::memory_order_relaxed);

            if (isSet) {
                auto dummy = system::error_code{}; // not error
                _handler(dummy); // set after wait
                return;
            }
        }

        assert(oldState == State::canceled); // cancel before set and wait
    }

    void cancel() {
        State oldState = State::not_set;
        bool isCancel = _state.compare_exchange_strong(
            oldState,
            State::canceled,
            std::memory_order_release,
            std::memory_order_acquire); // see change of handler if current state is not_set_consumer_waiting

        if (isCancel) {
            return; // cancel before wait
        }
        else if (oldState == State::not_set_consumer_waiting) {
            // wait win
            isCancel = _state.compare_exchange_strong(
                oldState,
                State::canceled,
                std::memory_order_relaxed,
                std::memory_order_relaxed);

            if (isCancel) {
                system::error_code ec = error::operation_aborted;
                _handler(ec); // cancel after wait, but before
                return;
            }
        }

        assert(oldState == State::set); // set before wait and cancel
    }
};

} // namespace boost::asio::awaitable_ext
