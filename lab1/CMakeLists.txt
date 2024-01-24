cmake_minimum_required(VERSION 3.0.0)

# set(HEADERS ${CMAKE_SOURCE_DIR}/lab1)

add_executable(client client.cpp socket.cpp)
# target_compile_options(client PRIVATE -frtti)

add_executable(server server.cpp socket.cpp)
# target_compile_options(server PRIVATE -frtti)