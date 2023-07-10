#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <chrono>
#include <iostream>

using namespace boost::asio::experimental::awaitable_operators;

auto async_main(std::chrono::seconds timeout) -> boost::asio::awaitable<void>
{
    auto executor = co_await boost::asio::this_coro::executor;
    auto timer = boost::asio::steady_timer(executor, timeout);
    auto signal = boost::asio::signal_set(executor, SIGINT, SIGTERM);
    auto result = co_await(
        signal.async_wait(boost::asio::use_awaitable) ||
        timer.async_wait(boost::asio::use_awaitable)
    );
    if (result.index() == 0) {
        std::cout << "signal finished first: " << std::get<0>(result) << std::endl;
    }
    else if (result.index() == 1) {
        std::cout << "timer finished first" << std::endl;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <timeout>" << std::endl;
        return EXIT_FAILURE;
    }
    auto timeout = std::chrono::seconds(std::stoul(argv[1]));
    auto ioContext = boost::asio::io_context();
    boost::asio::co_spawn(ioContext, async_main(timeout),
                          [](std::exception_ptr ex){ if (ex) std::rethrow_exception(ex); });
    ioContext.run();
    return EXIT_SUCCESS;
}
