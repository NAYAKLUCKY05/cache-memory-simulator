cmake_minimum_required(VERSION 3.16)

project(cache_memory_simulator VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(cache_simulator
    src/main.c
    src/config.c
    src/parser.c
    src/cache.c
    src/statistics.c
    src/simulator.c
)

target_include_directories(cache_simulator PRIVATE include)

if(MSVC)
    target_compile_options(cache_simulator PRIVATE /W4 /WX)
else()
    target_compile_options(cache_simulator PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME sample_trace_smoke_test
        COMMAND cache_simulator --trace ${CMAKE_CURRENT_SOURCE_DIR}/traces/sample_trace.txt
    )
endif()
