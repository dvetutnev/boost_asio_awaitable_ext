#include "nats_coro.h" // ICLient::eof
#include "subscribe.h"
#include "queue.h"
#include "utils.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/coro.hpp>

#include <boost/lexical_cast.hpp>

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
    };

    auto ioContext = io_context();
    co_spawn(ioContext, producer(), rethrow_handler);
    co_spawn(ioContext, consumer(), rethrow_handler);
    ioContext.run();

    std::vector<std::string> expected = {"a", "b", "c"};
    BOOST_CHECK_EQUAL_COLLECTIONS(std::begin(result), std::end(result),
                                  std::begin(expected), std::end(expected));
}

BOOST_AUTO_TEST_CASE(transfer)
{
    constexpr std::size_t iterationCount = 1'000'000;
    std::uint64_t result = 0;

    auto [front, back] = make_queue_sp<Message>(64);

    auto producer = [&]() -> awaitable<void>
    {
        auto to_message = [](std::size_t i)
        {
            auto payload = boost::lexical_cast<std::string>(i);
            return make_message(std::format("MSG 2w.px 92 {}\r\n{}\r\n",
                                            payload.size(),
                                            payload));
        };

        for (auto i = 1uz; i <= iterationCount; i++) {
            co_await front.push(to_message(i));
        }
        co_await front.push(Message{});
    };

    auto consumer = [&](std::uint64_t& sum) -> awaitable<void>
    {
        auto sub = subscription(co_await this_coro::executor,
                                std::move(back));
        while (auto msg = co_await sub.async_resume(use_awaitable)) {
            sum += boost::lexical_cast<std::uint64_t>(msg->payload());
        }
    };

    auto ioContext = io_context();
    co_spawn(ioContext, producer(), rethrow_handler);
    co_spawn(ioContext, consumer(result), rethrow_handler);
    ioContext.run();

    constexpr std::uint64_t expectedResult =
        static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result == expectedResult);
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace nats_coro::test
