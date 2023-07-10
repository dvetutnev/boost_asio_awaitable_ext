#include "nats_coro.h"

#include <boost/mysql.hpp>
#include <boost/asio.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <iostream>

using namespace std::string_view_literals;

constexpr auto natsUrl = "nats://token@localhost:4222"sv;
constexpr auto mysqlHost = "localhost"sv;
constexpr auto mysqlUser = "root"sv;
constexpr auto mysqlPassword = "password"sv;

constexpr auto use_awaitable_nothrow = boost::asio::as_tuple(boost::asio::use_awaitable);

auto connect2mysql() -> boost::asio::awaitable<boost::mysql::tcp_connection>
{
    auto executor = co_await boost::asio::this_coro::executor;

    auto resolver = boost::asio::ip::tcp::resolver(executor);
    auto endpoints = co_await resolver.async_resolve(mysqlHost,
                                                     boost::mysql::default_port_string,
                                                     boost::asio::use_awaitable);

    auto params = boost::mysql::handshake_params(mysqlUser,
                                                 mysqlPassword);
    boost::system::error_code ec;
    boost::mysql::diagnostics diag;

    auto conn = boost::mysql::tcp_connection(executor);
    std::tie(ec) = co_await conn.async_connect(*endpoints.begin(),
                                               params,
                                               diag,
                                               use_awaitable_nothrow);
    boost::mysql::throw_on_error(ec, diag);
    co_return conn;
}

auto async_main(std::string_view subject) -> boost::asio::awaitable<void>
{
    auto mqClient = co_await nats_coro::createClient(natsUrl);
    auto dbClient = co_await connect2mysql();

    auto process = [&](nats_coro::Message msg) -> boost::asio::awaitable<void>
    {
        auto query = msg.payload();
        auto replyTo = msg.head().reply_to();

        boost::system::error_code ec;
        boost::mysql::diagnostics diag;
        boost::mysql::results result;

        std::tie(ec) = co_await dbClient.async_query(query,
                                                     result,
                                                     diag,
                                                     use_awaitable_nothrow);
        boost::mysql::throw_on_error(ec, diag);
        co_await mqClient->publish(replyTo, result.rows()[0].at(0).as_string());
    };

    using Handler = std::function<boost::asio::awaitable<void>(nats_coro::Message)>;
    auto wrapper = [&](nats_coro::Message msg, Handler handler) -> boost::asio::awaitable<void>
    {
        auto executor = co_await boost::asio::this_coro::executor;
        auto replyTo = std::string(msg.head().reply_to());

        try {
            co_await handler(std::move(msg));
        }
        catch (const std::exception& ex) {
            co_spawn(executor,
                [&](std::string replyTo, std::string err) -> boost::asio::awaitable<void>
                {
                    co_await mqClient->publish(replyTo, err);
                }(std::move(replyTo), ex.what()),
                [](std::exception_ptr ex)
                {
                    if (ex) std::rethrow_exception(ex);
                });
        }
    };

    auto accept = [&](std::string_view subject, Handler handler) -> boost::asio::awaitable<void>
    {
        auto executor = co_await boost::asio::this_coro::executor;
        auto [sub, unsub] = co_await mqClient->subscribe(executor, subject);
        while (auto msg = co_await sub.async_resume(boost::asio::use_awaitable))
        {
            co_spawn(executor,
                     wrapper(std::move(*msg), handler),
                     [](std::exception_ptr ex)
                     {
                         try { if (ex) std::rethrow_exception(ex); }
                         catch (const std::exception& ex) { std::cerr << "Exception wrapper/process: " << ex.what() << std::endl; }
                     });
        }
        co_await unsub();
        std::cout << "accept stopped" << std::endl;
    };

    auto executor = co_await boost::asio::this_coro::executor;
    co_spawn(executor,
             accept(subject, process),
             [](std::exception_ptr ex)
             {
                 try { if (ex) std::rethrow_exception(ex); }
                 catch (const std::exception& ex) { std::cerr << "Exception accept: " << ex.what() << std::endl; }
             });

    auto signal = boost::asio::signal_set(executor, SIGINT, SIGTERM);

    auto [order, ex, _, __] = co_await boost::asio::experimental::make_parallel_group(
        co_spawn(executor, mqClient->run(), boost::asio::deferred),
        signal.async_wait(boost::asio::deferred))
        .async_wait(boost::asio::experimental::wait_for_one(),
                    boost::asio::use_awaitable);

    if (order[0] == 0 && ex) {
        std::rethrow_exception(ex);
    }

    std::cout << "service stopped" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <subject>" << std::endl;
        return EXIT_FAILURE;
    }
    auto ioContext = boost::asio::io_context();
    boost::asio::co_spawn(ioContext, async_main(argv[1]),
                          [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    ioContext.run();
    return EXIT_SUCCESS;
}
