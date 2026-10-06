# Architecture

## Modules

```text
main.c ──> simulator.c ──> parser.c      (reads the trace, one line at a time)
                │
                ├────────> cache.c       (lookup, replacement, write handling)
                │
                └────────> statistics.c  (counters, rates, AMAT)

config.c is used by all of them for defaults, validation and policy names.
```

| Module | Job |
|---|---|
| `main` | Parses command-line options into a `SimulatorConfig`. |
| `config` | Defaults, validation, enum <-> name conversion. |
| `parser` | Opens the trace and calls a callback for every `R`/`W` entry. It never loads the whole file. |
| `cache` | Owns the cache lines. `cache_access()` handles one access and returns what happened. |
| `statistics` | Turns access results into counts, rates and AMAT. |
| `simulator` | Connects the parser to the cache and statistics, and prints the report. |

The cache doesn't know about statistics, and the parser doesn't know about the
cache. `cache_access()` returns a `CacheAccessResult` that describes the access
(hit, evicted, writeback, ...), and the simulator passes it to the statistics.
This is also what makes the cache easy to unit test.

## Data layout

```text
Cache
  lines[]  one flat array of set_count * ways CacheLine entries
           set s = lines[s * ways ... s * ways + ways - 1]
  set_count, ways, offset_bits, index_bits
  replacement_policy, write_policy, write_allocate
  clock       incremented on every access (timestamps for LRU/FIFO)
  rng_state   xorshift32 state for Random

CacheLine
  valid, dirty, tag
  last_used   clock value of the last hit or fill   (LRU)
  loaded_at   clock value when the block was filled (FIFO)
```

All lines are in one `calloc` instead of one allocation per set. That means
one allocation to check and free, and the lines of a set sit next to each other
in memory.

## One access, step by step

1. Split the address: `offset = addr & (block-1)`,
   `set = (addr >> offset_bits) & (sets-1)`, `tag = addr >> (offset_bits + index_bits)`.
2. Look through the ways of that set for `valid && tag == tag`.
3. **Hit**: update `last_used`. On a write, set `dirty` (write-back) or count a
   memory write (write-through).
4. **Miss**: a write with no-write-allocate goes straight to memory and stops
   here. Otherwise choose a way: an empty one if any, else the policy's victim.
   If the victim was dirty, count a write-back. Then load the new tag.

## Address decomposition example

1 KiB cache, 16-byte blocks, direct-mapped gives 64 sets, 4 offset bits and
6 index bits:

```text
0x12345678 = ...0100 1000 1101 0001 0101 | 100111 | 1000
                     tag = 0x48D15        set=0x27  off=0x8
```

This exact case is checked in `tests/test_main.c`.
