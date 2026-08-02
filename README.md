# Cache Memory Simulator

A configurable CPU cache simulator written in portable C11. It consumes a
memory-address trace, models a single cache level, and reports hit/miss
statistics for direct-mapped, set-associative, and fully associative layouts.

The project is intentionally small enough to read end to end, while using the
same separation of concerns found in systems software: input parsing,
configuration validation, cache state management, statistics, and simulation
orchestration are independent modules.

## Motivation

Cache behavior is central to CPU performance, but it is often explained only
with diagrams or opaque simulators. This repository provides an inspectable
implementation that makes tag/index/offset decomposition and cache mapping
policies concrete. It is designed as a portfolio project for embedded systems,
computer architecture, and systems software interviews.

## Features

- Direct-mapped, set-associative, and fully associative cache organizations
- Configurable cache size, block size, and associativity
- 32-bit hexadecimal memory trace input
- Tag, set-index, and block-offset address decomposition
- Dynamic cache allocation with cleanup on allocation failures
- Hit/miss counters and percentage-based statistics
- Strict configuration and trace validation
- Cross-platform CMake build for Linux and Windows

## Architecture

```text
                 +-------------------+
                 |      main.c       |
                 | command-line CLI  |
                 +---------+---------+
                           |
                 +---------v---------+
                 |   simulator.c     |
                 | orchestration     |
                 +---+-----------+---+
                     |           |
        +------------v--+     +--v-------------+
        |   parser.c     |     |    cache.c     |
        | trace reader   |     | cache engine   |
        +----------------+     +--+-------------+
                                |
                       +--------v---------+
                       |  statistics.c    |
                       | counters / rates |
                       +------------------+

                 +-------------------+
                 |     config.c      |
                 | defaults/validation|
                 +-------------------+
```

## Repository layout

```text
cache-memory-simulator/
├── include/                 # Public module interfaces
├── src/                     # C11 implementations and CLI entry point
├── traces/                  # Small and larger example address traces
├── tests/                   # Build and validation guide
├── docs/                    # Architecture and design documentation
├── CMakeLists.txt           # Cross-platform build definition
├── LICENSE                  # MIT License
└── README.md
```

## Build

Requirements: CMake 3.16 or newer and a C11-capable compiler (GCC, Clang, or
MSVC).

### Linux / macOS

```sh
cmake -S . -B build
cmake --build build
```

### Windows PowerShell

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The executable is `build/cache_simulator` on single-configuration generators;
Visual Studio generators place it under a configuration directory such as
`build/Release/cache_simulator.exe`.
## Project Workflow

```text
Memory Trace
      │
      ▼
Address Parser
      │
      ▼
Configuration Validation
      │
      ▼
Address Decomposition
(Tag | Index | Offset)
      │
      ▼
Cache Lookup
      │
      ▼
Hit / Miss Decision
      │
      ▼
Cache Update
      │
      ▼
Statistics Engine
      │
      ▼
Performance Report
```
## Usage

```text
cache_simulator --trace <file> [--cache-size <bytes>] [--block-size <bytes>] [--associativity <n>]
```

Defaults: 32 KiB cache, 64-byte blocks, associativity 1, and 32-bit addresses.
All cache dimensions must be positive powers of two. Cache size must be evenly
divisible by block size, and associativity must divide the total line count.

```sh
# Run the default direct-mapped cache.
./build/cache_simulator --trace traces/sample_trace.txt

# Model a 32 KiB, 64-byte, 4-way set-associative cache.
./build/cache_simulator --trace traces/sample_trace_large.txt \
  --cache-size 32768 --block-size 64 --associativity 4

# Model a fully associative 1 KiB cache with 64-byte blocks (16 lines).
./build/cache_simulator --trace traces/sample_trace.txt \
  --cache-size 1024 --block-size 64 --associativity 16
```

Trace files contain one unsigned 32-bit hexadecimal byte address per line.
Blank lines and lines beginning with `#` are ignored.

```text
# Instruction and data accesses
0x00000010
0x00000020
0x00000040
```

## Example output

Running the supplied small trace with defaults produces:

```text
Cache Memory Simulator Report
=============================
Cache type:      Direct-Mapped
Cache size:      32768 bytes
Block size:      64 bytes
Associativity:   1
Address width:   32 bits
Total accesses:  7
Hits:            2
Misses:          5
Hit rate:        28.57%
Miss rate:       71.43%
```

## Supported cache organizations

| Organization | Configuration | Lookup behavior |
|---|---|---|
| Direct-mapped | Associativity = 1 | One indexed cache line is checked. |
| Set-associative | 1 < associativity < line count | Every way in the indexed set is checked. |
| Fully associative | Associativity = total line count | The one set contains every cache line. |

On a miss, the engine selects the first invalid way. When a set is full,
Version 1.0 deterministically overwrites way 0. This deliberate placeholder
makes behavior reproducible while keeping future replacement-policy work
isolated.

## Design decisions

- **Generic set engine:** all three organizations use one set-and-way access
  path rather than organization-specific duplicate code.
- **Dynamic allocation:** the simulator allocates exactly the requested number
  of sets and lines at run time.
- **Streaming parser:** traces are processed one address at a time instead of
  being loaded entirely into memory.
- **Defensive boundaries:** each module validates its inputs and returns a
  diagnostic through its existing API.

More detail is available in [Architecture](docs/ARCHITECTURE.md) and
[Design notes](docs/DESIGN.md).

## Performance Evaluation

The simulator was evaluated using identical memory traces while varying the
cache organization. The results demonstrate how cache associativity influences
overall hit and miss rates.

| Cache Organization | Hit Rate | Miss Rate |
|--------------------|---------:|----------:|
| Direct-Mapped | 46.67% | 53.33% |
| 2-Way Set Associative | 50.00% | 50.00% |
| 4-Way Set Associative | 53.33% | 46.67% |
| Fully Associative | 53.33% | 46.67% |

### Observation

Increasing associativity reduces conflict misses by allowing multiple cache
blocks to occupy the same set. For the supplied benchmark, performance
improves from Direct-Mapped to 4-Way Set Associative. The Fully Associative
configuration produces the same result because the selected workload does not
generate additional conflicts beyond those already resolved by the 4-Way
organization.

## Key Learnings

During this project I implemented and explored:

- Cache memory organization
- Tag, Index and Offset decomposition
- Direct-Mapped caches
- Set-Associative caches
- Fully Associative caches
- Dynamic memory allocation in C
- Modular software architecture
- Command-line application development
- Performance analysis using cache hit and miss statistics

## Technologies Used

- C11
- CMake
- GCC / Clang
- Modular Software Design
- Computer Architecture
- Cache Memory
- Memory Address Translation
- Performance Analysis
- Git
- GitHub

## Why This Project Matters

Modern processors rely heavily on cache memory to reduce average memory access
latency. Before implementing a cache in hardware, architects frequently use
behavioral simulators to evaluate alternative organizations and replacement
strategies. This project reproduces the logical behavior of a CPU cache in
software, allowing different cache configurations to be evaluated using the
same memory-access trace.

## Future roadmap

- Configurable LRU, FIFO, and random replacement policies
- Write-through and write-back behavior with dirty-bit tracking
- Multi-level cache hierarchy and memory latency modeling
- Read/write operation traces and policy-aware statistics
- Automated unit tests and benchmark traces
- Optional interactive visualization or GUI

See [Future improvements](docs/FUTURE_IMPROVEMENTS.md) for the full roadmap.

## Testing

Run the bundled smoke test after building:

```sh
ctest --test-dir build --output-on-failure
```

The manual validation scenarios and expected small-trace result are documented
in [tests/TESTING.md](tests/TESTING.md).

## License

Released under the [MIT License](LICENSE).
