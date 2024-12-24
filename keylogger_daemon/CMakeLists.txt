cmake_minimum_required(VERSION 3.28.3)
project(keylogger_daemon)

set(CMAKE_CXX_STANDARD 17)

add_executable(client client.cpp)
add_executable(server server.cpp)
