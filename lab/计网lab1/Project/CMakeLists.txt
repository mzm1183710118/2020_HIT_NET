cmake_minimum_required(VERSION 3.16)
project(untitled)
link_libraries(ws2_32)
set(CMAKE_CXX_STANDARD 14)

add_executable(myProxy myProxy.cpp)