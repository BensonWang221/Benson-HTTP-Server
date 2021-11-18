cmake_minimum_required(VERSION "3.0")

project(BENSON_HTTP_SERVER)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -DTEST -g -O2 -Wall")

add_subdirectory(src)

add_executable(app main.cpp)

target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(app pthread httpserver)