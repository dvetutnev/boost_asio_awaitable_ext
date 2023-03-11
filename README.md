# boost_asio_awaitable_ext

Additional synchronization primitives for [boost::asio::awaitable](https://www.boost.org/doc/libs/1_79_0/doc/html/boost_asio/reference/awaitable.html) [coroutines](https://www.boost.org/doc/libs/1_79_0/doc/html/boost_asio/overview/composition/cpp20_coroutines.html)

# Requirements
 * C++23 complier (GCC 12)
 * CMake
 * Conan 1.x

# Build and tests
```sh
cmake -S ./ -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
ctest --test-dir build
```
