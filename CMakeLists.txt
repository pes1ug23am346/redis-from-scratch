cmake_minimum_required(VERSION 3.16)

project(redis_from_scratch)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(redis-server
    src/main.cpp
)

add_executable(test_avl
    tests/test_avl.cpp
)

add_executable(test_zset
    tests/test_zset.cpp
)

add_executable(test_server
    tests/test_server.cpp
)
