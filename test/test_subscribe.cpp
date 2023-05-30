#include "nats_coro.h" // ICLient::eof
#include "subscribe.h"
#include "queue.h"
#include "utils.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/coro.hpp>

#include <boost/test/unit_test.hpp>

#include <format>

namespace nats_coro::test {

using experimental::coro;

BOOST_AUTO_TEST_SUITE(tests_Subscription);

BOOST_AUTO_TEST_CASE(_)
{
    auto [front, back] = make_queue_sp<Message>(64);

    auto producer = [&]() -> awaitable<void>
    {
        for (char c : std::string("abc")) {
            auto msg = make_message(std::format("MSG kk.ss 42 1\r\n{}\r\n",
                                                c));
            co_await front.push(std::move(msg));
        }
        co_await front.push(Message{});
    };

    std::vector<std::string> result;
    auto consumer = [&]() -> awaitable<void>
    {
        auto sub = subscription(co_await this_coro::executor,
                                std::move(back));
        while (auto msg = co_await sub.async_resume(use_awaitable)) {
            result.emplace_back(msg->payload());
        }
        co_return;
    };

    auto ioContext = io_context();
    co_spawn(ioContext, producer(), rethrow_handler);
    co_spawn(ioContext, consumer(), rethrow_handler);
    ioContext.run();

    std::vector<std::string> expected = {"a", "b", "c"};
    BOOST_CHECK_EQUAL_COLLECTIONS(std::begin(result), std::end(result),
                                  std::begin(expected), std::end(expected));
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace nats_coro::test
