cmake_minimum_required(VERSION 3.16)
project(ImAlive C)

set(CMAKE_C_STANDARD 99)

include_directories(include)
file(GLOB INCLUDES "include/*.h")
file(GLOB LIBSRCS "libs/*.c")

add_executable(ImAlive_client ${LIBSRCS} ${INCLUDES} im_alive_client.c )
target_link_libraries(ImAlive_client pthread)

add_executable(ImAlive_server ${LIBSRCS} ${INCLUDES} client_state.c im_alive_server.c )
target_link_libraries(ImAlive_server pthread websockets)
