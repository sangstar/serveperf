cmake_minimum_required(VERSION 4.0)
project(serveperf C)

find_package(CURL REQUIRED)
set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")

add_executable(serveperf src/main.c
        src/parse.c
        src/parse.h
        src/http_tools.c
        src/http_tools.h
        src/rb.c
        src/rb.h
        src/types.c
        src/types.h
        src/debug.c
        src/debug.h
        src/threads.c
        src/threads.h
        src/shared_ptr.h
)
find_package(Threads REQUIRED)
target_link_libraries(serveperf PRIVATE CURL::libcurl Threads::Threads)
