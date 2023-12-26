cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(Dust
        VERSION 0.0.1
        DESCRIPTION "A jit-compiler for brainfuck"
        LANGUAGES C CXX)

find_package(LLVM REQUIRED)

# TODO: Change this into a more modern solution
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall -Wextra -Wpedantic -Og -g3 \
-fsanitize=address -Wuseless-cast -Wcast-align -Wcast-qual -Wduplicated-branches \
-Wduplicated-cond -Wempty-body -Wfloat-equal -Wformat=2 -Winit-self -Wmissing-braces")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall -Wdisabled-optimization -O3 -DNDEBUG")

add_executable(dust dust.cpp ds_utils.cpp)

target_link_libraries(dust LLVM)
