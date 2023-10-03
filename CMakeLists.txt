cmake_minimum_required(VERSION 3.26)
project(tcp2 C)

set(CMAKE_C_STANDARD 11)

include_directories(src)

add_executable(tcp2
        src/log.c
        src/log.h
        src/main.c
        src/tcp_client.c
        src/tcp_client.h)
