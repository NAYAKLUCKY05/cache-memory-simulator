# Testing

## Running the tests

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Six CTest entries run:

| Test | What it checks |
|---|---|
| `unit_tests` | `tests/test_main.c`, see below |
| `run_small_trace` | the small trace runs and reports 2 hits |
| `run_read_write_trace` | the read/write trace runs on a 2-way cache |
| `conflicts_need_8_ways` | experiment 1: 8 colliding blocks hit 99.90% with 8 ways |
| `column_walk_thrashes` | experiment 5: the column walk gets 0.00% even with 8 ways |
| `reject_bad_config` | a 48-byte block size is rejected with a non-zero exit |

## Unit tests

`tests/test_main.c` uses no external framework. Each test is a function, and a
`CHECK()` macro counts failures. The tests are:

- config validation (good config, non power-of-two, too many ways)
- address decode of `0x12345678` into tag/set/offset
- direct-mapped conflict: `0x000` and `0x400` evict each other
- LRU: after A, B, A, C, the cache keeps A and evicts B
- FIFO: the same sequence evicts A, because A was loaded first
- Random: two caches with the same seed make identical choices
- write-back: a dirty line is counted as a write-back when evicted
- write-through + no-write-allocate: a write miss doesn't load the block
- parser: `R`/`W` prefixes, comments, blank lines, bad input
- statistics: hit rate and AMAT arithmetic

### Property tests

The last four tests don't check one specific answer. They check rules that
must hold for *every* design. Each one runs a pseudo-random read/write trace
of 5,000 accesses.

- **invariants, all configs**: 252 combinations of size, block size,
  ways, replacement policy, write policy and allocation. For each one it checks:
  - every access is counted once
  - blocks fetched = evictions + valid lines left
  - write-backs never exceed evictions
  - with write-allocate, every miss fetches a block; without it, only read
    misses do
  - write-through never has dirty lines, and every write goes to memory
- **direct-mapped ignores policy**: with one way there is nothing to choose,
  so LRU, FIFO and Random must give identical results
- **write policy keeps hits**: with write-allocate, write-back and
  write-through keep the same blocks, so their hit counts must be equal
- **LRU: bigger is never worse**: LRU is a "stack algorithm", so a larger
  fully associative LRU cache can never have fewer hits

### Do the tests catch bugs?

To check, bugs were put into `src/cache.c` on purpose and the tests run
against each one:

| Bug introduced | Caught by |
|---|---|
| LRU timestamp not updated on a hit | LRU replacement |
| every eviction reported as a write-back | invariants, all configs |
| dirty bit not cleared when a new block is loaded | write-back |

## Memory checking

On Linux/macOS, build with sanitizers to catch leaks and out-of-bounds access:

```sh
cmake -S . -B build-asan -DCMAKE_C_FLAGS="-fsanitize=address,undefined -g"
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

## Things to try by hand

```sh
# Should fail with a clear message and exit code 1
./build/cache_simulator
./build/cache_simulator --trace traces/missing.txt
./build/cache_simulator --trace traces/sample_trace.txt --cache-size 1000
./build/cache_simulator --trace traces/sample_trace.txt --policy mru

# Compare policies on the same trace
for p in lru fifo random; do
  ./build/cache_simulator --trace traces/sample_trace_mixed.txt \
      --cache-size 1024 --associativity 4 --policy $p --format csv | tail -1
done
```
