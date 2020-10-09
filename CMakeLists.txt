cmake_minimum_required(VERSION 3.15)
project(ImAlive C)

set(CMAKE_C_STANDARD 99)

include_directories(include)
file(GLOB INCLUDES "include/*.h")
file(GLOB LIBSRCS "libs/*.c")

add_executable(ImAlive_client ${LIBSRCS} ${INCLUDES} im_alive_client.c )
add_executable(ImAlive_server ${LIBSRCS} ${INCLUDES} im_alive_server.c client_state.c wsserver.c)

if(APPLE)
    target_compile_options(ImAlive_server PRIVATE -I/usr/local/include -I/usr/local/opt/openssl/include)
    target_link_options(ImAlive_server PRIVATE -L/usr/local/lib -L/usr/local/opt/openssl/lib)
    target_link_libraries(
            ImAlive_server
            "-framework IOKit"
            "-framework CoreFoundation"
            /usr/local/opt/libwebsockets/lib/libwebsockets.a
            /usr/local/opt/libuv/lib/libuv.a
            /usr/local/opt/libevent/lib/libevent.a
            /usr/local/opt/openssl@1.1/lib/libcrypto.a
            /usr/local/opt/openssl@1.1/lib/libssl.a
    )
    target_link_libraries(
            ImAlive_client
            "-framework IOKit"
            "-framework CoreFoundation"
    )
else()
    find_package(Threads REQUIRED)
    target_link_libraries(ImAlive_client pthreads )
    target_link_libraries(ImAlive_server pthreads websockets crypto ssl)
endif()