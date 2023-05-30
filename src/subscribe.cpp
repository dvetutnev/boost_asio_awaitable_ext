#include "subscribe.h"
#include "nats_coro.h" // IClient::eof

namespace nats_coro {

auto subscription(any_io_executor executor, SubQueueBack queue) -> coro<Message>
{
    bool end = false;
    do {
        auto range = co_await co_spawn(executor, queue.get(), deferred);
        for (auto seq : range) {
            Message& msg = queue[seq];
            if (msg == IClient::eof) { // remove prefix 'IClient' in future
                end = true;
                break;
            }
            co_yield std::move(msg);
        }
    } while (!end);
}


} // namespace nats_coro
