cmake_minimum_required(VERSION 3.17)
project(ImAlive C)

set(CMAKE_C_STANDARD 99)

add_executable(ImAlive_client im_alive_client.c im_alive.h)
add_executable(ImAlive_server im_alive_server.c im_alive.h)