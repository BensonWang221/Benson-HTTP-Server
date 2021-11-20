cmake_minimum_required(VERSION 3.0)

project(LIBEVENT_TEST)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -O2 -Wall")

add_executable(libevent_test libevent_http_server.cpp libevent_test.cpp Timer.cpp ThreadPool.cpp)

target_link_libraries(libevent_test pthread event)