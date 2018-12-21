cmake_minimum_required(VERSION 3.12)
project(Projekt C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

find_package (Threads)

add_executable(Projekt main.c)
target_link_libraries (Projekt ${CMAKE_THREAD_LIBS_INIT})
