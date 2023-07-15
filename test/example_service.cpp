#include "nats_coro.h"
#include "event.h"

#include <boost/mysql.hpp>
#include <boost/asio.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <boost/noncopyable.hpp>

#include <iostream>

using namespace std::string_view_literals;

constexpr auto natsUrl = "nats://token@localhost:4222"sv;
constexpr auto mysqlHost = "localhost"sv;
constexpr auto mysqlUser = "root"sv;
constexpr auto mysqlPassword = "password"sv;

constexpr auto use_awaitable_nothrow = boost::asio::as_tuple(boost::asio::use_awaitable);

struct TaskCounter : boost::noncopyable
{
    struct Task : boost::noncopyable
    {
        explicit Task(TaskCounter& counter) : _counter{&counter} { _counter->increment(); }
        ~Task() { if (_counter) _counter->decrement(); }

        Task(Task&& other) { _counter = std::exchange(other._counter, nullptr); }
        Task& operator=(Task&& other) { _counter = std::exchange(other._counter, nullptr); return *this; }

    private:
        TaskCounter* _counter;
    };

    boost::asio::awaitable<void> wait_all_completion() {
        if (_count.load(std::memory_order_acquire)) {
            co_await _event.wait(boost::asio::use_awaitable);
        }
    }

    Task make_task() { return Task{*this}; }

private:
    void increment() { _count.fetch_add(1, std::memory_order_relaxed); }
    void decrement() {
        auto prev = _count.fetch_sub(1, std::memory_order_acquire);
        if (prev == 1) { // last task
            _event.set();
        }
    }

    std::atomic_size_t _count = 0;
    boost::asio::awaitable_ext::Event _event;
};

auto connect2mysql(boost::asio::ssl::context& sslContext)
    -> boost::asio::awaitable<std::shared_ptr<boost::mysql::tcp_ssl_connection>>
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

    auto conn = std::make_shared<boost::mysql::tcp_ssl_connection>(executor, sslContext);
    std::tie(ec) = co_await conn->async_connect(*endpoints.begin(),
                                                params,
                                                diag,
                                                use_awaitable_nothrow);
    boost::mysql::throw_on_error(ec, diag);
    co_return conn;
}

auto async_main(std::string_view subject,
                boost::asio::ssl::context& sslContext) -> boost::asio::awaitable<void>
{
    auto mqClient = co_await nats_coro::createClient(natsUrl);
    auto dbClient = co_await connect2mysql(sslContext);

    auto taskCounter = TaskCounter();

    auto process = [&](nats_coro::Message msg) -> boost::asio::awaitable<void>
    {
        auto query = msg.payload();
        auto replyTo = msg.head().reply_to();

        boost::system::error_code ec;
        boost::mysql::diagnostics diag;
        boost::mysql::results result;

        std::tie(ec) = co_await dbClient->async_query(query,
                                                      result,
                                                      diag,
                                                      use_awaitable_nothrow);
        boost::mysql::throw_on_error(ec, diag);
        co_await mqClient->publish(replyTo, result.rows()[0].at(0).as_string());
    };

    using Handler = std::function<boost::asio::awaitable<void>(nats_coro::Message)>;
    auto wrapper = [&](nats_coro::Message msg,
                       Handler handler,
                       TaskCounter::Task task) -> boost::asio::awaitable<void>
    {
        auto executor = co_await boost::asio::this_coro::executor;
        auto replyTo = std::string(msg.head().reply_to());

        try {
            co_await handler(std::move(msg));
        }
        catch (const std::exception& ex) {
            co_spawn(executor,
                [&](std::string replyTo,
                    std::string err,
                    TaskCounter::Task) -> boost::asio::awaitable<void>
                {
                    co_await mqClient->publish(replyTo, err);
                } (std::move(replyTo), ex.what(), std::move(task)),
                [](std::exception_ptr ex)
                {
                    if (ex) std::rethrow_exception(ex);
                });
        }
    };

    auto accept = [&](std::string_view subject,
                      Handler handler,
                      TaskCounter::Task) -> boost::asio::awaitable<void>
    {
        auto executor = co_await boost::asio::this_coro::executor;
        auto [sub, unsub] = co_await mqClient->subscribe(executor, subject);
        while (auto msg = co_await sub.async_resume(boost::asio::use_awaitable))
        {
            co_spawn(executor,
                     wrapper(std::move(*msg),
                             handler,
                             taskCounter.make_task()),
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
             accept(subject,
                    process,
                    taskCounter.make_task()),
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

    co_await taskCounter.wait_all_completion();

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
    auto sslContext = boost::asio::ssl::context(boost::asio::ssl::context::tls_client);
    boost::asio::co_spawn(ioContext, async_main(argv[1], sslContext),
                          [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    ioContext.run();
    return EXIT_SUCCESS;
}
