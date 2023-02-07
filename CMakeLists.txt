cmake_minimum_required(VERSION 3.20)

# Temporary compatibility for Clang 14
add_compile_options("-std=c++2b")
#set(CMAKE_CXX_STANDARD 23)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

include(detect_project_name)
detect_project_name(project_name)
message(STATUS "Project name: ${project_name}")
project(${project_name} CXX)

include(conan)
conan_cmake_autodetect(settings)
conan_cmake_install(
    PATH_OR_REFERENCE ${CMAKE_SOURCE_DIR}/conanfile.txt
    BUILD missing
    SETTINGS ${settings})

add_compile_definitions(BOOST_ASIO_HAS_CO_AWAIT=1)

find_package(Boost REQUIRED)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)
target_link_libraries(${PROJECT_NAME} INTERFACE Boost::boost)

enable_testing()
add_subdirectory(test)
