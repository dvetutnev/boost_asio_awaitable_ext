# boost_asio_awaitable_ext

Additional synchronization primitives for [boost::asio::awaitable](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/awaitable.html) [coroutines](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/overview/composition/cpp20_coroutines.html).

Based on [cppcoro](https://github.com/lewissbaker/cppcoro).

# Requirements
 * C++23 complier (GCC 13)
 * CMake
 * Boost

# Build and tests
```bash
git clone https://github.com/dvetutnev/boost_asio_awaitable_ext.git
cd boost_asio_awitable_ext
```
All commands run in folder of repository.

## Conan 2
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

Install depependecies
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

## Nix
Get shell with all dependecies
```bash
nix-shell --pure
```

Build and tests
```bash
[nix-shell]$ cmake -B build -D CMAKE_BUILD_TYPE=Debug
[nix-shell]$ cmake --build build
[nix-shell]$ ctest --test-dir build
```

ThreadSanitizer tests
```bash
[nix-shell]$ git submodule update --init
[nix-shell]$ cmake -B build -D TSAN_TESTS=ON
[nix-shell]$ cmake --build build
[nix-shell]$ ctest --test-dir build
```

# NATS environment

## Run NATS server in Docker
```bash
$ docker run --name local-nats -p 4222:4222 -p 8222:8222 nats -m 8222 --auth token
```

## [NATS Utility](https://docs.nats.io/using-nats/nats-tools/nats_cli)
```bash
$ nix-shell -p natscli
```

```bash
[nix-shell:~]$ nats sub --server=token@localhost a.b
08:44:25 Subscribing on a.b 
[#1] Received on "a.b"
45
```

```bash
[nix-shell:~]$ nats --server=token@localhost pub a.b 45
08:45:30 Published 2 bytes to "a.b"
```
