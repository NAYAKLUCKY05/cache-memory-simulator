# Design Notes

Why things are built the way they are.

## One code path for every organization

Direct-mapped, set-associative and fully associative caches differ only in
how many sets and ways there are. The engine always does "find set, search its
ways". Direct-mapped is `ways = 1` and fully associative is `sets = 1`. This
avoids three separate implementations that could drift apart.

## Power-of-two sizes only

Real caches use power-of-two sizes so the set index and offset can be taken
straight from address bits with a shift and a mask. The simulator works the
same way, so other sizes are rejected in `config_validate()`.

## Timestamps instead of LRU lists or bits

Each access increments a 64-bit clock, and each line stores the clock value of
its last use (LRU) and of its fill (FIFO). Choosing a victim scans the ways for
the smallest value. This costs O(ways) per miss, which is fine for software
because associativity is small, and it is easy to verify. Hardware usually
uses pseudo-LRU (a tree of bits) instead, because true LRU costs too much at
high associativity.

## Seeded random

`Random` uses xorshift32 instead of `rand()`. `rand()` gives different
sequences on different C libraries, which would make results differ between
Windows and Linux. xorshift32 is three lines of code and gives the same
sequence everywhere for the same `--seed`.

## Write policies

- **Write-back + write-allocate** (the default): a store marks the line dirty.
  Memory is updated only when that line is evicted (counted as a write-back).
- **Write-through**: every store also goes to memory (counted as a direct write).
  Lines are never dirty.
- **No-write-allocate**: a write miss does not load the block and only writes
  memory. This is usually combined with write-through.

"Dirty at end" in the report counts lines that would still need writing back
if the cache were flushed.

## Errors

Functions that can fail return `bool` and write a message into a buffer that
the caller provides. There's no global error state, no `exit()` inside modules,
and no heap allocation for messages. `main()` decides what to print and which
exit code to return.

## Output formats

The text report is for people. CSV and JSON exist so that you can loop over
configurations in a shell script and collect the results:

```sh
for a in 1 2 4 8 16; do
  ./build/cache_simulator --trace traces/sample_trace_mixed.txt \
      --cache-size 1024 --associativity $a --format csv | tail -1
done
```

With `--verbose` and CSV/JSON, the per-access lines go to stderr so stdout
remains valid CSV/JSON.
