cmake_minimum_required(VERSION 3.16)

project(cache_memory_simulator VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    set(WARNING_FLAGS /W4 /WX)
    # fopen/strtoull are fine here; silence MSVC's "use the _s version" warnings.
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    set(WARNING_FLAGS -Wall -Wextra -Wpedantic -Werror)
endif()

# Everything except main() goes in a library so the tests can link it.
add_library(cache_core STATIC
    src/config.c
    src/parser.c
    src/cache.c
    src/statistics.c
    src/simulator.c
)
target_include_directories(cache_core PUBLIC include)
target_compile_options(cache_core PRIVATE ${WARNING_FLAGS})

add_executable(cache_simulator src/main.c)
target_link_libraries(cache_simulator PRIVATE cache_core)
target_compile_options(cache_simulator PRIVATE ${WARNING_FLAGS})

include(CTest)
if(BUILD_TESTING)
    add_executable(unit_tests tests/test_main.c)
    target_link_libraries(unit_tests PRIVATE cache_core)
    target_compile_options(unit_tests PRIVATE ${WARNING_FLAGS})
    add_test(NAME unit_tests COMMAND unit_tests)

    set(TRACES ${CMAKE_CURRENT_SOURCE_DIR}/traces)

    add_test(NAME run_small_trace
             COMMAND cache_simulator --trace ${TRACES}/sample_trace.txt)
    set_tests_properties(run_small_trace PROPERTIES
             PASS_REGULAR_EXPRESSION "Hits: +2 ")

    add_test(NAME run_read_write_trace
             COMMAND cache_simulator --trace ${TRACES}/read_write_trace.txt
                     --cache-size 256 --block-size 64 --associativity 2)

    # Two experiment results that should never change (see docs/RESULTS.md).
    add_test(NAME conflicts_need_8_ways
             COMMAND cache_simulator --trace ${TRACES}/workloads/conflict_stride.txt
                     --associativity 8)
    set_tests_properties(conflicts_need_8_ways PROPERTIES
             PASS_REGULAR_EXPRESSION "Hit rate: +99.90%")

    add_test(NAME column_walk_thrashes
             COMMAND cache_simulator --trace ${TRACES}/workloads/matrix_col_major.txt
                     --associativity 8)
    set_tests_properties(column_walk_thrashes PROPERTIES
             PASS_REGULAR_EXPRESSION "Hit rate: +0.00%")

    add_test(NAME reject_bad_config
             COMMAND cache_simulator --trace ${TRACES}/sample_trace.txt --block-size 48)
    set_tests_properties(reject_bad_config PROPERTIES WILL_FAIL TRUE)
endif()
