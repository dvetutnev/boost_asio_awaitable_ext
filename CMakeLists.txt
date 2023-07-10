cmake_minimum_required(VERSION 3.20)

# Temporary compatibility for Clang
add_compile_options("-std=c++2b")
# set(CMAKE_CXX_STANDARD 23) set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

project(boost_asio_awaitable_ext CXX)

option(TSAN_TESTS "Build ThreadSanitizer tests" OFF)
option(ASIO_HANDLER_TRACKING "Enable Boost.ASIO handler tracking" OFF)

add_compile_definitions(BOOST_ASIO_HAS_CO_AWAIT=1)
if(ASIO_HANDLER_TRACKING)
  add_compile_definitions(BOOST_ASIO_ENABLE_HANDLER_TRACKING=1)
endif()

find_package(Boost REQUIRED COMPONENTS json url unit_test_framework)
find_package(OpenSSL REQUIRED)

add_library(
  ${PROJECT_NAME} src/nats_coro.cpp src/queue.cpp src/connect_to_nats.cpp
                  src/subscribe.cpp src/message.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_link_libraries(${PROJECT_NAME} PUBLIC Boost::json Boost::url)

if(TSAN_TESTS)
  add_library(disruptorplus INTERFACE)
  target_include_directories(disruptorplus INTERFACE disruptorplus/include)
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(test)
endif()
