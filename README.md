# Cache Memory Simulator

A command-line CPU cache simulator in C11. It reads a trace of memory
accesses, runs them through a configurable cache, and reports hit/miss
behaviour, memory traffic and average memory access time (AMAT).

Its purpose is to evaluate a cache design before it is built: try different
sizes, associativities and policies on the same workload and compare the
results. See [docs/RESULTS.md](docs/RESULTS.md) for five such design studies.

```text
$ ./build/cache_simulator --trace traces/read_write_trace.txt \
      --cache-size 256 --block-size 64 --associativity 2

Configuration
  Organization:    Set-Associative
  Cache size:      256 bytes
  Block size:      64 bytes
  Associativity:   2-way (2 sets)
  Address split:   tag 25 | index 1 | offset 6 bits
  Replacement:     LRU
  Write policy:    Write-Back, write-allocate
  Timing:          hit 1 cycles, miss penalty 100 cycles

Results
  Accesses:        21 (15 reads, 6 writes)
  Hits:            6 (5 read, 1 write)
  Misses:          15
  Hit rate:        28.57%
  Miss rate:       71.43%
  Evictions:       11
  AMAT:            72.43 cycles

Memory traffic
  Blocks fetched:  15
  Write-backs:     4
  Direct writes:   0
  Dirty at end:    1
```

## Features

- Direct-mapped, set-associative and fully associative caches (any
  power-of-two size, block size and associativity)
- Replacement policies: **LRU**, **FIFO** and **Random** (seeded, so runs are
  repeatable)
- Read and write accesses, with **write-back** (dirty bits) or
  **write-through**, and write-allocate or no-write-allocate
- Timing model with hit time and miss penalty, reporting **AMAT**
- Memory traffic counters: blocks fetched, dirty write-backs, direct writes
- `--verbose` mode that prints the tag/set/offset split and hit/miss/eviction
  for every access
- Text, CSV or JSON reports, so results can go straight into a spreadsheet or
  script
- Unit tests, property tests over 252 cache configurations, and end-to-end
  tests, all run through CTest

## Build

You need CMake 3.16+ and a C11 compiler (GCC, Clang, MinGW or MSVC).

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

On Windows with Visual Studio the executable ends up in
`build\Debug\cache_simulator.exe` (or `Release`). Add `-C Debug` to the
`ctest` command.

## Usage

```text
cache_simulator --trace <file> [options]

Cache geometry:
  --cache-size <bytes>       total capacity (default 32768)
  --block-size <bytes>       line size (default 64)
  --associativity <n>        ways per set; 1 = direct-mapped (default 1)

Policies:
  --policy <lru|fifo|random> replacement policy (default lru)
  --seed <n>                 seed for the random policy (default 1)
  --write-policy <wb|wt>     write-back or write-through (default wb)
  --no-write-allocate        don't load a block on a write miss

Timing:
  --hit-time <cycles>        cache hit time (default 1)
  --miss-penalty <cycles>    extra cycles on a miss (default 100)

Output:
  --format <text|csv|json>   report format (default text)
  --verbose                  print every access (hit/miss, evictions)
```

Some examples:

```sh
# 4-way, 1 KiB cache, FIFO instead of LRU
./build/cache_simulator --trace traces/sample_trace_mixed.txt \
    --cache-size 1024 --associativity 4 --policy fifo

# Fully associative (16 lines of 64 bytes = 1 KiB)
./build/cache_simulator --trace traces/sample_trace_large.txt \
    --cache-size 1024 --associativity 16

# Write-through, no-write-allocate, CSV output
./build/cache_simulator --trace traces/read_write_trace.txt \
    --cache-size 256 --associativity 2 --write-policy wt --no-write-allocate --format csv

# Watch each access
./build/cache_simulator --trace traces/sample_trace.txt --verbose
```

`--verbose` output looks like this (default 32 KiB direct-mapped cache):

```text
R 0x00000010  tag=0x0      set=0     off=16  MISS
R 0x00000020  tag=0x0      set=0     off=32  HIT
R 0x00000040  tag=0x0      set=1     off=0   MISS
R 0x00008010  tag=0x1      set=0     off=16  MISS  evict tag=0x0
R 0x00000010  tag=0x0      set=0     off=16  MISS  evict tag=0x1
```

`0x00000010` and `0x00008010` keep throwing each other out of set 0. That is
a conflict miss, and a 2-way cache removes it.

## Trace format

One access per line. The address is 32-bit hex, with or without `0x`. An
optional `R` or `W` in front marks a read or a write. Without one, the access
is a read, so plain address lists still work. Blank lines and `#` comments are
skipped.

```text
# load then store
R 0x00001000
W 0x00001004
0x00002000
```

## Design experiments

[docs/RESULTS.md](docs/RESULTS.md) uses larger workloads (8,000 to 40,000
accesses each) to answer real design questions:

| Question | Finding |
|---|---|
| How many ways? | 8 colliding blocks need exactly 8 ways: 0% hits at 4-way, 99.9% at 8-way. 16-way adds nothing. |
| How big? | For a 16 KiB hot set, 32 KiB is the knee (88.6%). 64 KiB adds only 0.5%. |
| Which replacement? | LRU wins on hot/cold data (88.9% vs 82.8% FIFO), but loses to Random on a loop slightly bigger than the cache (93.8% vs 97.5%). |
| Which write policy? | Write-back writes 10× less to memory than write-through when data is updated repeatedly. |
| Hardware or software? | A column walk over a 128×128 matrix gets 0% hits even 8-way. Padding each row by one element makes it 91.7% even direct-mapped. |

## How it works

Every byte address is split into three fields:

```text
| tag | set index | block offset |
        log2(sets)  log2(block size) bits
```

The set index picks a set, and every way in that set is checked for a valid
line with a matching tag. Direct-mapped and fully associative caches are the
same code: direct-mapped means 1 way per set, and fully associative means a
single set.

On a miss, an empty way is used if there is one. Otherwise the replacement
policy picks a victim:

- **LRU** stores a timestamp of the last access on each line and evicts the
  oldest.
- **FIFO** stores the time each line was loaded and evicts the oldest.
- **Random** uses a small xorshift32 generator with a fixed seed, so the
  results are repeatable.

Writes depend on the write policy. Write-back sets a dirty bit, and the line
is written to memory only when it is evicted. Write-through sends every store
to memory immediately. With no-write-allocate, a write miss bypasses the cache
completely.

AMAT is calculated as `hit time + miss rate × miss penalty`.

## Project layout

```text
include/   module headers
src/
  main.c        command-line parsing
  config.c      defaults, validation, policy names
  parser.c      trace file reader
  cache.c       cache storage, lookup, replacement, write handling
  statistics.c  counters, rates, AMAT
  simulator.c   ties it together and prints reports
tests/     unit and property tests (test_main.c), testing notes
traces/    small sample traces
  workloads/   larger design-study workloads
tools/     gen_traces.py (makes the workloads), run_experiments.py
docs/      architecture, design notes, RESULTS.md and raw results/
```

More detail is in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and
[docs/DESIGN.md](docs/DESIGN.md).

## Possible next steps

- Multi-level hierarchy (L1 + L2) with per-level AMAT
- Prefetching (next-line, stride)
- Reading Valgrind/Lackey or ChampSim trace formats

## License

MIT, see [LICENSE](LICENSE).
