cmake_minimum_required(VERSION 3.17)
project(ImAlive C)

set(CMAKE_C_STANDARD 99)

include_directories(include)
file(GLOB INCLUDES "include/*.h")
file(GLOB LIBSRCS "libs/*.c")

add_executable(ImAlive_client im_alive_client.c ${LIBSRCS} ${INCLUDES} )
add_executable(ImAlive_server im_alive_server.c ${LIBSRCS} ${INCLUDES})