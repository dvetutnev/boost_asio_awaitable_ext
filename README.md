# boost_asio_awaitable_ext

Additional synchronization primitives for [boost::asio::awaitable](https://www.boost.org/doc/libs/1_79_0/doc/html/boost_asio/reference/awaitable.html) [coroutines](https://www.boost.org/doc/libs/1_79_0/doc/html/boost_asio/overview/composition/cpp20_coroutines.html)

# Requirements
 * C++23 complier (GCC 12)
 * CMake
 * Conan 2

# Build and tests

Detect platform (optional)
```bash
conan profile detect --force
CC and CXX: gcc, g++ 
Found gcc 12.2
gcc>=5, using the major as version
gcc C++ standard library: libstdc++11
Detected profile:
[settings]
arch=x86_64
build_type=Release
compiler=gcc
compiler.cppstd=gnu17
compiler.libcxx=libstdc++11
compiler.version=12
os=Linux
```

Install deps
```bash
conan install -s build_type=Debug -of build --build missing conanfile.py
```

Build and tests
```bash
cmake --list-presets
Available configure presets:

  "conan-debug" - 'conan-debug' config

cmake --preset conan-debug
cmake --build build
ctest --preset conan-debug
```

ThreadSanitizer tests
```bash
git submodule update --init
cmake --preset conan-debug -D TSAN_TESTS=ON
cmake --build build
ctest --preset conan-debug
```
