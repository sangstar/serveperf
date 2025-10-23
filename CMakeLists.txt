cmake_minimum_required(VERSION 4.0)
project(serveperf C)

find_package(CURL REQUIRED)
set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_executable(serveperf src/main.c
        src/parse.c
        src/parse.h
        src/http_tools.c
        src/http_tools.h
        src/rb.c
        src/rb.h
        src/io_tools.c
        src/io_tools.h
        src/debug.c
        src/debug.h
        src/threads.c
        src/threads.h
)
find_package(Threads REQUIRED)
target_link_libraries(serveperf PRIVATE CURL::libcurl Threads::Threads)
