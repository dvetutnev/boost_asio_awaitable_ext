cmake_minimum_required(VERSION 3.20)

# Temporary compatibility for Clang
add_compile_options("-std=c++2b")
# set(CMAKE_CXX_STANDARD 23)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

project(boost_asio_awaitable_ext CXX)

add_compile_definitions(BOOST_ASIO_HAS_CO_AWAIT=1)

find_package(Boost REQUIRED COMPONENTS unit_test_framework)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)
target_link_libraries(${PROJECT_NAME} INTERFACE Boost::boost)

option(TSAN_TESTS "Build ThreadSanitizer tests" OFF)
if(TSAN_TESTS)
  add_library(disruptorplus INTERFACE)
  target_include_directories(disruptorplus INTERFACE disruptorplus/include)
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(test)
endif()
