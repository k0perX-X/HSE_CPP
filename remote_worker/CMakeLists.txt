cmake_minimum_required(VERSION 3.28.3)
project(remote_worker)

set(CMAKE_CXX_STANDARD 20)

add_executable(client_runner client.cpp)
add_executable(server_runner server.cpp)
add_executable(test test.cpp)
