#pragma once

#include "sequence_traits.h"

#include <atomic>
#include <limits>
#include <vector>
#include <memory>
#include <algorithm>
#include <concepts>

namespace boost::asio::awaitable_ext::test {

template<std::unsigned_integral TSequence>
struct MockAwaiter
{
    const TSequence targetSequence;
    MockAwaiter* next;

    explicit MockAwaiter(TSequence s) : targetSequence{s}, next{nullptr}, busy{false} {}
    void resume(TSequence) {
        next = nullptr;
        busy.store(false, std::memory_order_release); // Not use single total order
    }

private:
    template<std::unsigned_integral, typename, typename>
    friend struct MockAwaitersStorage;
    std::atomic_bool busy;
};

template<std::unsigned_integral TSequence,
         typename Traits = SequenceTraits<TSequence>,
         typename Awaiter = MockAwaiter<TSequence>>
struct MockAwaitersStorage
{
    MockAwaitersStorage() {
        constexpr std::size_t countAwaiters = std::numeric_limits<TSequence>::max() + 1;
        std::generate_n(std::back_inserter(_storage), countAwaiters,
                        [s = TSequence{}] mutable { return std::make_unique<Awaiter>(s++); });
    }

    // Return nullptr if no free awaiter
    Awaiter* get_upper(TSequence target) {
        TSequence current = target;
        do {
            Awaiter* awaiter = _storage[current].get();
            bool captured = !(awaiter->busy.exchange(true, std::memory_order_acquire)); // Not use single total order
            if (captured) {
                return awaiter;
            }
        } while (Traits::precedes(target, ++current));

        return nullptr;
    }

    Awaiter* get_lower(TSequence target) {
        TSequence current = target;
        do {
            Awaiter* awaiter = _storage[current].get();
            bool captured = !(awaiter->busy.exchange(true, std::memory_order_acquire)); // Not use single total order
            if (captured) {
                return awaiter;
            }
        } while (Traits::precedes(--current, target));

        return nullptr;
    }

private:
    // Atomic MockAwaiter::busy can`t moved/copied, unique_ptr is wrapper
    std::vector<std::unique_ptr<Awaiter>> _storage;
};

} // namespace boost::asio::awaitable_ext::test
