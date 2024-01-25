cmake_minimum_required(VERSION 3.0.0)

find_package(Threads REQUIRED)

add_executable(server server.cpp)

add_executable(client client.cpp)

target_link_libraries(server ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries(client ${CMAKE_THREAD_LIBS_INIT})

add_executable(testserver testserver.cpp)
