cmake_minimum_required(VERSION 3.21)
project(Pokemon C)

set(CMAKE_C_STANDARD 99)

add_executable(Pokemon main.c heap.c heap.h)
