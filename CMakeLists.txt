cmake_minimum_required(VERSION 3.16)

project(cpp_game_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build unit tests" ON)

find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(spdlog QUIET)
find_package(absl QUIET)

add_subdirectory(src)
add_subdirectory(bot)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
