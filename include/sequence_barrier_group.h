#pragma once

#include "sequence_barrier.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <algorithm>
#include <vector>

namespace boost::asio::awaitable_ext {

template<std::unsigned_integral TSequence = std::size_t,
         typename Traits = SequenceTraits<TSequence>>
class SequenceBarrierGroup
{
public:
    using BarrierRef = std::reference_wrapper<SequenceBarrier<TSequence, Traits>>;

    SequenceBarrierGroup(std::vector<BarrierRef> barriers)
        :
        _barriers{std::move(barriers)}
    {
        assert(_barriers.size() > 0);
    }

    SequenceBarrierGroup(const SequenceBarrierGroup&) = delete;
    SequenceBarrierGroup& operator=(const SequenceBarrierGroup&) = delete;

    [[nodiscard]] awaitable<TSequence> wait_until_published(TSequence targetSequence) const
    {
        using experimental::make_parallel_group;
        using experimental::wait_for_all;

        auto executor = co_await this_coro::executor;

        auto makeOperation = [executor, targetSequence](BarrierRef barrier)
        {
            // As args, not capture
            auto coro = [](BarrierRef barrier, TSequence targetSequence) -> awaitable<TSequence>
            {
                co_return co_await barrier.get().wait_until_published(targetSequence);
            };

            return co_spawn(executor, coro(barrier, targetSequence), deferred);
        };

        using Operation = decltype(makeOperation(_barriers.front()));
        std::vector<Operation> operations;

        operations.reserve(_barriers.size());
        for (BarrierRef barrier : _barriers) {
            operations.push_back(makeOperation(barrier));
        }

        auto [order, exceptions, published] =
            co_await make_parallel_group(std::move(operations))
                .async_wait(wait_for_all(), use_awaitable);

        (void)order;
        auto isThrow = [](const std::exception_ptr& ex) -> bool { return !!ex; };
        if (auto firstEx = std::find_if(std::begin(exceptions), std::end(exceptions), isThrow);
            firstEx != std::end(exceptions))
        {
            if (std::any_of(firstEx, std::end(exceptions), isThrow)) {
                throw multiple_exceptions(*firstEx);
            } else {
                std::rethrow_exception(*firstEx);
            }
        }

        auto it = std::min_element(std::begin(published), std::end(published),
                                   [](TSequence a, TSequence b) { return Traits::precedes(a, b); } );
        co_return *it;
    }

private:
    const std::vector<BarrierRef> _barriers;
};

} // namespace boost::asio::awaitable_ext
