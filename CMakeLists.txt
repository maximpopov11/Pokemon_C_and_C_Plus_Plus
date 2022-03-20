cmake_minimum_required(VERSION 3.21)
project(Pokemon C)

set(CMAKE_C_STANDARD 99)
#set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS} -L/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/usr/lib -lncurses" )

add_executable(Pokemon main.c heap.c heap.h)

target_link_libraries(Pokemon ncurses)