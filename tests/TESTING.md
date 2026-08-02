# Testing Guide

## Prerequisites

Install CMake 3.16 or newer and a compiler with C11 support. The CMake target
enables strict warnings as errors:

```text
-Wall -Wextra -Wpedantic -Werror
```

MSVC uses the equivalent `/W4 /WX` options.

## Build

From the repository root:

```sh
cmake -S . -B build
cmake --build build
```

On Windows with a multi-configuration generator:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Smoke test

The CMake configuration registers the small trace as a CTest smoke test:

```sh
ctest --test-dir build --output-on-failure
```

This confirms the executable can initialize the default cache, read the trace,
complete a simulation, and return a successful exit status.

## Functional validation

### Default direct-mapped run

```sh
./build/cache_simulator --trace traces/sample_trace.txt
```

Expected key values:

```text
Cache type:      Direct-Mapped
Total accesses:  7
Hits:            2
Misses:          5
Hit rate:        28.57%
Miss rate:       71.43%
```

Why: `0x00000010` and `0x00000020` share a 64-byte block. The addresses
`0x00000010` and `0x00008010` map to the same default-cache set, producing
deterministic conflicts.

### Set-associative run

```sh
./build/cache_simulator --trace traces/sample_trace_large.txt \
  --cache-size 32768 --block-size 64 --associativity 4
```

Verify that the report identifies `Set-Associative`, completes successfully,
and reports `total accesses: 30`.

### Fully associative run

```sh
./build/cache_simulator --trace traces/sample_trace.txt \
  --cache-size 1024 --block-size 64 --associativity 16
```

Verify that the report identifies `Fully Associative`.

## Negative tests

Each command should fail with a clear diagnostic and a nonzero exit status.

```sh
# Missing required trace option
./build/cache_simulator

# Trace file does not exist
./build/cache_simulator --trace traces/does_not_exist.txt

# Cache size is not a power of two
./build/cache_simulator --trace traces/sample_trace.txt --cache-size 1000

# Block size does not divide cache size
./build/cache_simulator --trace traces/sample_trace.txt --block-size 128

# Associativity exceeds the number of lines
./build/cache_simulator --trace traces/sample_trace.txt --associativity 1024
```

For a parser validation check, create a temporary trace containing `not_an_address`
and confirm that the output identifies the invalid trace line. Do not add that
temporary file to version control.

## Manual code-quality check

The configured build treats warnings as errors. For a direct GCC/Clang check:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  src/main.c src/config.c src/parser.c src/cache.c src/statistics.c src/simulator.c \
  -o cache_simulator
```

Run the default trace after compiling. If the command succeeds and the expected
statistics match, the core end-to-end path is validated.
