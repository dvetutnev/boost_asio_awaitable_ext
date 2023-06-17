#include "nats_coro.h"
#include "event.h"
#include "schedule.h"
#include "utils.h"
#include "connect_to_nats.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/coro.hpp>
#include <boost/asio/experimental/use_coro.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/url.hpp>

#include <boost/test/unit_test.hpp>

namespace nats_coro::test {

using namespace awaitable_ext;
using namespace experimental::awaitable_operators;
using namespace buffer_literals;

using experimental::coro;
using experimental::use_coro;

using namespace std::chrono_literals;

namespace {

constexpr auto natsUrl = std::string_view{"nats://token@localhost:4222"};
constexpr auto mockUrl = std::string_view{"nats://token@localhost:4223"};

auto mock_nats(unsigned port) -> awaitable<ip::tcp::socket>
{
    using namespace boost::asio;

    auto executor = co_await this_coro::executor;
    auto endpoint = ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
    auto acceptor = ip::tcp::acceptor(executor, endpoint);
    auto socket = ip::tcp::socket(executor);
    co_await acceptor.async_accept(socket, use_awaitable);

    auto srvConfig = R"({"server_id":"NCP2P7REDTV5AAHJ3BN24CGZJ3ZEMB4WYBDGP7PRGKUNCH3PKTIYJQBD","server_name":"NCP2P7REDTV5AAHJ3BN24CGZJ3ZEMB4WYBDGP7PRGKUNCH3PKTIYJQBD","version":"2.9.17","proto":1,"git_commit":"4f2c9a5","go":"go1.19.9","host":"0.0.0.0","port":4222,"headers":true,"auth_required":true,"max_payload":1048576,"client_id":30,"client_ip":"172.17.0.1"})";
    std::string info = std::format("INFO {}\r\n", srvConfig);
    co_await async_write(socket,
                         buffer(info),
                         use_awaitable);

    std::string reply;
    co_await async_read_until(socket,
                              dynamic_buffer(reply),
                              "\r\n",
                              use_awaitable);
    BOOST_TEST(reply.starts_with("CONNECT "));

    co_return socket;
}

auto mock_nats(std::string_view url) ->awaitable<ip::tcp::socket>
{
    boost::url_view r = * boost::urls::parse_uri(url);
    return mock_nats(r.port_number());
}

auto line_reader(ip::tcp::socket& socket) -> coro<std::string>
{
    streambuf buf;
    for (;;) {
        std::size_t size = co_await async_read_until(socket,
                                                     buf,
                                                     "\r\n",
                                                     use_coro);
        auto begin = buffers_begin(buf.data());
        co_yield {begin,
                  begin + size};
        buf.consume(size);
    }
}

} // Anonymous namespace

BOOST_AUTO_TEST_SUITE(nats_coro);

BOOST_AUTO_TEST_CASE(reply_on_ping)
{
    Event stop;

    auto client = [&]() -> awaitable<void>
    {
        auto client = co_await createClient(mockUrl);
        auto result = co_await(client->run() ||
                                stop.wait(use_awaitable));
        BOOST_TEST(result.index() == 1); // stop win
    };

    auto server = [&]() -> awaitable<void>
    {
        auto socket = co_await mock_nats(mockUrl);
        auto reader = line_reader(socket);
        for (int i = 0; i < 3; i++) {
            co_await async_write(socket,
                                 "PING\r\n"_buf,
                                 use_awaitable);
            auto msg = co_await reader.async_resume(use_awaitable);
            BOOST_REQUIRE(msg);
            BOOST_TEST(*msg == "PONG\r\n");
        }
        stop.set();
    };

    auto ioContext = io_context();
    co_spawn(ioContext, client(), rethrow_handler);
    co_spawn(ioContext, server(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(first_publish)
{
    auto main = [&]() -> awaitable<void>
    {
        Event stop;

        auto client = co_await createClient(natsUrl);
        co_spawn(
            co_await this_coro::executor,
            [&]() -> awaitable<void> { auto result = co_await(client->run() || stop.wait(use_awaitable)); BOOST_TEST(result.index() == 1); },
            rethrow_handler);

        co_await client->publish("a.b", "First publish");
        co_await async_sleep(100ms);
        stop.set();
    };

    auto ioContext = io_context();
    co_spawn(ioContext, main(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(first_transfer)
{
    Event start, stop;

    auto consumer = [&]() -> awaitable<void>
    {
        auto client = co_await createClient(natsUrl);
        auto [sub, _] = co_await client->subscribe("f.t");
        auto subWrap = [&]() -> awaitable<std::optional<Message>>
        {
            co_await async_sleep(50ms); // wait delivery 'SUB' to NATS
            start.set();
            co_return co_await sub.async_resume(use_awaitable);
        };
        auto res = co_await(client->run() || subWrap());
        stop.set();
        BOOST_REQUIRE(res.index() == 1);

        auto msg = std::get<1>(std::move(res));
        BOOST_REQUIRE(msg.has_value());
        BOOST_TEST(msg->head().subject() == "f.t");
        BOOST_TEST(msg->head().payload_size() == 4);
        BOOST_TEST(msg->payload() == "data");
    };

    auto producer = [&]() -> awaitable<void>
    {
        co_await start.wait(use_awaitable);
        auto client = co_await createClient(natsUrl);
        co_await client->publish("f.t", "data");
        auto res = co_await(client->run() ||
                             stop.wait(use_awaitable));
        BOOST_TEST(res.index() == 1); // stop win
    };

    auto ioContext = io_context();
    co_spawn(ioContext, consumer(), rethrow_handler);
    co_spawn(ioContext, producer(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(payload_contains_delimiter)
{
    Event start, stop;

    auto consumer = [&]() -> awaitable<void>
    {
        auto client = co_await createClient(natsUrl);
        auto [sub, _] = co_await client->subscribe("r.n");
        auto subWrap = [&]() -> awaitable<std::optional<Message>>
        {
            co_await async_sleep(50ms); // wait delivery 'SUB' to NATS
            start.set();
            co_return co_await sub.async_resume(use_awaitable);
        };
        auto res = co_await(client->run() || subWrap());
        stop.set();
        BOOST_REQUIRE(res.index() == 1);

        auto msg = std::get<1>(std::move(res));
        BOOST_REQUIRE(msg.has_value());
        BOOST_TEST(msg->head().subject() == "r.n");
        BOOST_TEST(msg->head().payload_size() == 7);
        BOOST_TEST(msg->payload() == "A\r\nB\r\nC");
    };

    auto producer = [&]() -> awaitable<void>
    {
        co_await start.wait(use_awaitable);
        auto client = co_await createClient(natsUrl);
        co_await client->publish("r.n", "A\r\nB\r\nC");
        auto res = co_await(client->run() ||
                             stop.wait(use_awaitable));
        BOOST_TEST(res.index() == 1); // stop win
    };

    auto ioContext = io_context();
    co_spawn(ioContext, consumer(), rethrow_handler);
    co_spawn(ioContext, producer(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(unsub)
{
    Event stop;

    auto client = [&]() -> awaitable<void>
    {
        auto client = co_await createClient(mockUrl);
        auto [sub, unsub] = co_await client->subscribe("s.u");
        co_await unsub();
        auto result = co_await(client->run() ||
                                stop.wait(use_awaitable));
        BOOST_TEST(result.index() == 1); // stop win
    };

    auto server = [&]() -> awaitable<void>
    {
        auto socket = co_await mock_nats(mockUrl);
        auto reader = line_reader(socket);

        std::optional<std::string> msg;
        std::string subscribeId;

        msg = co_await reader.async_resume(use_awaitable);
        BOOST_REQUIRE(msg);
        {
            std::vector<std::string> chunks;
            boost::split(chunks, *msg, boost::algorithm::is_space());
            BOOST_TEST(chunks[0] == "SUB");
            BOOST_TEST(chunks[1] == "s.u");
            subscribeId = chunks[2];
            BOOST_TEST(!subscribeId.empty());
        }

        msg = co_await reader.async_resume(use_awaitable);
        BOOST_REQUIRE(msg);
        {
            std::vector<std::string> chunks;
            boost::split(chunks, *msg, boost::algorithm::is_space());
            BOOST_TEST(chunks[0] == "UNSUB");
            BOOST_TEST(chunks[1] == subscribeId);
        }

        stop.set();
    };

    auto ioContext = io_context();
    co_spawn(ioContext, client(), rethrow_handler);
    co_spawn(ioContext, server(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(unsub_dtor)
{
    Event stop;

    auto client = [&]() -> awaitable<void>
    {
        auto client = co_await createClient(mockUrl);
        auto [sub, unsub] = co_await client->subscribe("d.u.s");
        auto wrapUnsub = std::make_optional(std::move(unsub));
        wrapUnsub.reset();
        auto result = co_await(client->run() ||
                                stop.wait(use_awaitable));
        BOOST_TEST(result.index() == 1); // stop win
    };

    auto server = [&]() -> awaitable<void>
    {
        auto socket = co_await mock_nats(mockUrl);
        auto reader = line_reader(socket);

        std::optional<std::string> msg;
        std::string subscribeId;

        msg = co_await reader.async_resume(use_awaitable);
        BOOST_REQUIRE(msg);
        {
            std::vector<std::string> chunks;
            boost::split(chunks, *msg, boost::algorithm::is_space());
            BOOST_TEST(chunks[0] == "SUB");
            BOOST_TEST(chunks[1] == "d.u.s");
            subscribeId = chunks[2];
            BOOST_TEST(!subscribeId.empty());
        }

        msg = co_await reader.async_resume(use_awaitable);
        BOOST_REQUIRE(msg);
        {
            std::vector<std::string> chunks;
            boost::split(chunks, *msg, boost::algorithm::is_space());
            BOOST_TEST(chunks[0] == "UNSUB");
            BOOST_TEST(chunks[1] == subscribeId);
        }

        stop.set();
    };

    auto ioContext = io_context();
    co_spawn(ioContext, client(), rethrow_handler);
    co_spawn(ioContext, server(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(uniquie_subscribe_unsub)
{
    Event start, stop;

    auto consumer = [&]() -> awaitable<void>
    {
        auto client = co_await createClient(natsUrl);
        auto [sub1, unsub1] = co_await client->subscribe("u.s");
        auto [sub2, unsub2] = co_await client->subscribe("u.s");

        co_await unsub1();

        auto subWrap = [&]() -> awaitable<
                                    std::tuple<std::optional<Message>,
                                               std::optional<Message>>>
        {
            co_await async_sleep(50ms); // wait delivery 'SUB'/'UNSUB' to NATS
            start.set();
            co_return co_await(sub1.async_resume(use_awaitable) &&
                                sub2.async_resume(use_awaitable));
        };
        auto res = co_await(client->run() || subWrap());
        stop.set();
        BOOST_REQUIRE(res.index() == 1); // sub`s first
        auto [empty, msg] = std::get<1>(std::move(res));
        BOOST_REQUIRE(msg.has_value());
        BOOST_TEST(msg->head().subject() == "u.s");
        BOOST_TEST(msg->head().payload_size() == 2);
        BOOST_TEST(msg->payload() == "79");
        BOOST_TEST(!empty.has_value());
    };

    auto producer = [&]() ->awaitable<void>
    {
        auto client = co_await createClient(natsUrl);
        co_await start.wait(use_awaitable);
        co_await client->publish("u.s", "79");
        auto result = co_await(client->run() ||
                                stop.wait(use_awaitable));
        BOOST_TEST(result.index() == 1); // stop win
    };

    auto ioContext = io_context();
    co_spawn(ioContext, consumer(), rethrow_handler);
    co_spawn(ioContext, producer(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(uniquie_subscribe)
{
    Event start, stop;

    auto consumer = [&]() -> awaitable<void>
    {
        auto client = co_await createClient(natsUrl);
        auto [sub1, _1] = co_await client->subscribe("42.43");
        auto [sub2, _2] = co_await client->subscribe("42.43");

        auto subWrap = [&]() -> awaitable<
                                 std::tuple<std::optional<Message>,
                                            std::optional<Message>>>
        {
            co_await async_sleep(50ms); // wait delivery 'SUB'
            start.set();
            co_return co_await(sub1.async_resume(use_awaitable) &&
                                sub2.async_resume(use_awaitable));
        };
        auto res = co_await(client->run() || subWrap());
        stop.set();
        BOOST_REQUIRE(res.index() == 1); // sub`s first
        auto [msg1, msg2] = std::get<1>(std::move(res));
        BOOST_REQUIRE(msg1.has_value());
        BOOST_TEST(msg1->head().subject() == "42.43");
        BOOST_TEST(msg1->head().payload_size() == 3);
        BOOST_TEST(msg1->payload() == "444");
        BOOST_REQUIRE(msg2.has_value());
        BOOST_TEST(msg2->head().subject() == "42.43");
        BOOST_TEST(msg2->head().payload_size() == 3);
        BOOST_TEST(msg2->payload() == "444");
    };

    auto producer = [&]() ->awaitable<void>
    {
        auto client = co_await createClient(natsUrl);
        co_await start.wait(use_awaitable);
        co_await client->publish("42.43", "444");
        auto result = co_await(client->run() ||
                                stop.wait(use_awaitable));
        BOOST_TEST(result.index() == 1); // stop win
    };

    auto ioContext = io_context();
    co_spawn(ioContext, consumer(), rethrow_handler);
    co_spawn(ioContext, producer(), rethrow_handler);
    ioContext.run();
}

namespace {
struct Watchdog
{
    Watchdog(std::chrono::milliseconds timeout)
        :
        _timeout{timeout},
        _deadline{std::chrono::steady_clock::now() + timeout}
    {}

    awaitable<void> operator()()
    {
        auto timer = steady_timer(co_await this_coro::executor);
        auto now = std::chrono::steady_clock::now();
        while (_deadline > now)
        {
            timer.expires_at(_deadline);
            co_await timer.async_wait(use_awaitable);
            now = std::chrono::steady_clock::now();
        }
    }

    void touch() {
        _deadline = std::chrono::steady_clock::now() + _timeout;
    }

    std::chrono::milliseconds _timeout;
    std::chrono::steady_clock::time_point _deadline;
};
}

BOOST_AUTO_TEST_SUITE(shutdown)

BOOST_AUTO_TEST_CASE(test_Watchdog)
{
    auto timer = Watchdog(50ms);

    auto consumer = [&]() -> awaitable<void>
    {
        auto start = std::chrono::steady_clock::now();
        co_await timer();
        auto end = std::chrono::steady_clock::now();
        BOOST_TEST((end - start) > (50ms * 3));
        BOOST_TEST((end - start) < (50ms * 4));
    };

    auto producer = [&]() -> awaitable<void>
    {
        for (int i = 0; i < 3; i++)
        {
            co_await async_sleep(47ms);
            timer.touch();
        }
    };

    auto ioContext = io_context();
    co_spawn(ioContext, consumer(), rethrow_handler);
    co_spawn(ioContext, producer(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(flush_tx_queue, * boost::unit_test::disabled())
{
    std::size_t publishedCount = 0;
    auto timer = Watchdog(50ms);
    bool stopPublish = false;

    auto client = [&]() -> awaitable<void>
    {
        auto client = co_await createClient(mockUrl);
        auto publish = [&]() -> awaitable<void>
        {
            while (!stopPublish) {
                auto payload  = std::to_string(++publishedCount);
                co_await client->publish("p.s", payload);
                timer.touch();
            }
            co_await client->shutdown();
        };
        co_await(client->run() && publish());
    };

    auto server = [&]() -> awaitable<void>
    {
        auto socket = co_await mock_nats(mockUrl);
        co_await timer();
        stopPublish = true;
        BOOST_TEST_CHECKPOINT("timer fired, publishedCount=" << publishedCount);
        std::size_t receivedLinesCount = 0;
        std::optional<std::string> line;
        auto reader = line_reader(socket);
        do {
            line = co_await reader.async_resume(use_awaitable);
        } while(++receivedLinesCount < publishedCount * 2 &&
                 line.has_value());
        BOOST_TEST(line.has_value());
        auto lastVal = std::stoull(*line);
        BOOST_TEST(lastVal == publishedCount);
    };

    auto ioContext = io_context();
    co_spawn(ioContext, client(), rethrow_handler);
    co_spawn(ioContext, server(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(send_unsub)
{
    auto client = [&]() -> awaitable<void>
    {
        auto client = co_await createClient(mockUrl);
        auto [_, unsub] = co_await client->subscribe("s.u");
        co_await client->shutdown();
        co_await unsub();
        co_await client->run();
    };

    auto server = [&]() -> awaitable<void>
    {
        auto socket = co_await mock_nats(mockUrl);
        auto reader = line_reader(socket);
        std::optional<std::string> line;
        line = co_await reader.async_resume(use_awaitable);
        BOOST_REQUIRE(!!line);
        BOOST_TEST(line->starts_with("SUB s.u"));
        line = co_await reader.async_resume(use_awaitable);
        BOOST_REQUIRE(!!line);
        BOOST_TEST(line->starts_with("UNSUB"));
    };

    auto ioContext = io_context();
    co_spawn(ioContext, client(), rethrow_handler);
    co_spawn(ioContext, server(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(eof_sub)
{
    bool subStopping = false;
    auto main = [&]() -> awaitable<void>
    {
        auto client = co_await createClient(natsUrl);
        auto [sub, unsub] = co_await client->subscribe("e.s");
        auto subscribe = [&]() -> awaitable<void>
        {
            auto val = co_await sub.async_resume(use_awaitable);
            BOOST_TEST(!val);
            co_await unsub();
            subStopping = true;
        };
        co_await client->shutdown();
        co_await (client->run() && subscribe());
    };

    auto ioContext = io_context();
    co_spawn(ioContext, main(), rethrow_handler);
    ioContext.run();

    BOOST_TEST(subStopping);
}

BOOST_AUTO_TEST_CASE(disable_publish)
{
    auto main = [&]() -> awaitable<void>
    {
        auto client = co_await createClient(natsUrl);
        auto publish = [&]() -> awaitable<void>
        {
            BOOST_CHECK_EXCEPTION(co_await client->publish("d.p", "ff"),
                                  boost::system::system_error,
                                  [](const auto& ex){ return ex.code() == error::operation_aborted; });
        };
        co_await client->shutdown();
        co_await (client->run() && publish());
    };

    auto ioContext = io_context();
    co_spawn(ioContext, main(), rethrow_handler);
    ioContext.run();
}

BOOST_AUTO_TEST_SUITE_END(); // shutdown

BOOST_AUTO_TEST_CASE(transfer)
{
    constexpr std::size_t iterationCount = 1000;
    std::uint64_t result = 0;
    Event start;

    auto consumer= [&](std::uint64_t& sum) -> awaitable<void>
    {
        auto client = co_await createClient(natsUrl);
        auto subscribe = [&]() -> awaitable<void>
        {
            auto [sub, unsub] = co_await client->subscribe("y.g");
            start.set();
            while (auto msg = co_await sub.async_resume(use_awaitable))
            {
                auto val = boost::lexical_cast<std::uint64_t>(msg->payload());
                if (val == 0) {
                    break;
                }
                sum += val;
            }
            co_await unsub();
            co_await client->shutdown();
        };
        co_await (client->run() && subscribe());
    };

    auto producer = [&](std::size_t iterationCount) -> awaitable<void>
    {
        auto client = co_await createClient(natsUrl);
        auto publish = [&]() -> awaitable<void>
        {
            co_await start.wait(use_awaitable);
            for (std::size_t i = 1; i <= iterationCount; i++)
            {
                co_await client->publish("y.g", std::to_string(i));
            }
            co_await client->publish("y.g", "0");
            co_await client->shutdown();
        };
        co_await (client->run() && publish());
    };

    auto ioContext = io_context();
    co_spawn(ioContext, consumer(result), rethrow_handler);
    co_spawn(ioContext, producer(iterationCount), rethrow_handler);
    ioContext.run();

    constexpr std::uint64_t expectedResult =
        static_cast<std::uint64_t>(iterationCount) * static_cast<std::uint64_t>(1 + iterationCount) / 2;
    BOOST_TEST(result == expectedResult);
}

BOOST_AUTO_TEST_SUITE_END(); // nats_coro

} // namespace nats_coro::test
