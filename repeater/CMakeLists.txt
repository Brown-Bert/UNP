cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_CXX_COMPILER "/usr/bin/g++" )

add_compile_options(-O0)

add_library(library repeater.cpp)

add_executable(clientmain clientmain.cpp)
target_link_libraries(clientmain library -lpthread)

add_executable(relaymain relaymain.cpp)
target_link_libraries(relaymain library -lpthread)

add_executable(servermain servermain.cpp)
target_link_libraries(servermain library -lpthread)

add_executable(searchmain searchmain.cpp)
target_link_libraries(searchmain library -lpthread)

add_executable(testclient testclient.cpp)
add_executable(testserver testserver.cpp)
# add_executable(tt tt.cpp)
# 添加额外的参数
# target_compile_options(test PRIVATE -fcoroutines -std=c++20)
# message(STATUS "Launch Target Path: ${CMAKE_LAUNCH_TARGET_PATH}")
