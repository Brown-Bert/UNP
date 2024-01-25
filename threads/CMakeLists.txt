cmake_minimum_required(VERSION 3.0.0)


# find_library(Threads REQUIRED)
add_executable(test test.cpp)
target_link_libraries(test pthread)