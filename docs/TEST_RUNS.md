# Test Runs: Commands and Expected Results

Run these in the VS Code terminal from the project folder. Every number below
was produced by the current version. You should see exactly the same values,
because the simulator is deterministic and Random uses a fixed seed.

Defaults when not given: 32 KiB cache, 64-byte blocks, 1-way, LRU,
write-back + write-allocate, 1-cycle hit, 100-cycle miss penalty.

Prefix every command with `.\build\cache_simulator.exe`. That part is left out
below to keep the commands short.

---

## Run 0: one access at a time (start here)

```
--trace traces\example.txt --cache-size 256 --associativity 2 --verbose
```

Expected:
```
W 0x00001000  tag=0x20  set=0  off=0  MISS
R 0x00001040  tag=0x20  set=1  off=0  MISS
R 0x00001044  tag=0x20  set=1  off=4  HIT
R 0x00002000  tag=0x40  set=0  off=0  MISS
R 0x00003000  tag=0x60  set=0  off=0  MISS  evict tag=0x20 (write-back)
R 0x00001000  tag=0x20  set=0  off=0  MISS  evict tag=0x40
```
Section 4 of PROJECT_GUIDE.md explains every line.

---

## Run 1: how many ways are needed? (conflict misses)

| Command | Hits | Hit rate | Evictions | AMAT |
|---|---:|---:|---:|---:|
| `--trace traces\workloads\conflict_stride.txt --associativity 1` | 0 | 0.00% | 7999 | 101.00 |
| `--trace traces\workloads\conflict_stride.txt --associativity 4` | 0 | 0.00% | 7996 | 101.00 |
| `--trace traces\workloads\conflict_stride.txt --associativity 8` | 7992 | **99.90%** | 0 | **1.10** |

**Why:** 8 addresses map to the same set. Up to 4 ways they keep evicting
each other. With 8 ways all of them fit, so only the first 8 accesses miss
(Blocks fetched = 8). That's the jump from 0% to 99.9%, and AMAT drops from
101 to 1.1 cycles.

---

## Run 2: how big should the cache be? (capacity misses)

4-way, size varied, `hot_cold` workload (16 KiB of hot data):

| Command | Hit rate | AMAT |
|---|---:|---:|
| `--trace traces\workloads\hot_cold.txt --cache-size 8192 --associativity 4` | 39.55% | 61.46 |
| `--trace traces\workloads\hot_cold.txt --cache-size 16384 --associativity 4` | 75.02% | 25.98 |
| `--trace traces\workloads\hot_cold.txt --cache-size 32768 --associativity 4` | **88.62%** | 12.38 |
| `--trace traces\workloads\hot_cold.txt --cache-size 65536 --associativity 4` | 89.08% | 11.91 |

**Why:** the hit rate climbs until the hot data fits (32 KiB), then flattens.
64 KiB doubles the hardware for only +0.46%, so 32 KiB is the best choice.

---

## Run 3: which replacement policy?

8-way, 32 KiB:

| Command | Hit rate |
|---|---:|
| `--trace traces\workloads\hot_cold.txt --associativity 8 --policy lru` | **88.92%** |
| `--trace traces\workloads\hot_cold.txt --associativity 8 --policy fifo` | 82.81% |
| `--trace traces\workloads\hot_cold.txt --associativity 8 --policy random` | 83.31% |
| `--trace traces\workloads\cyclic_36k.txt --associativity 8 --policy lru` | 93.75% |
| `--trace traces\workloads\cyclic_36k.txt --associativity 8 --policy random` | **97.46%** |

**Why:** LRU keeps hot data because it is used often, so it wins on
`hot_cold`. On a loop slightly bigger than the cache, LRU always evicts the
block needed next, so Random wins. No policy is best everywhere.

---

## Run 4: write-back vs write-through

| Command | Direct writes | Dirty at end | Bytes written to memory |
|---|---:|---:|---:|
| `--trace traces\workloads\histogram.txt` | 0 | 128 | 128 × 64 = **8 KiB** |
| `--trace traces\workloads\histogram.txt --write-policy wt` | 20000 | 0 | 20000 × 4 = **80 KB** |

**Why:** the same 8 KiB table is written 20,000 times. Write-back keeps
those writes in the cache, so only the 128 dirty blocks ever need to go to
memory. Write-through sends every single store, about 10× more traffic. The
hit rate is the same (99.68%): the write policy changes *traffic*, not hits.

---

## Run 5: write-allocate vs no-write-allocate

| Command | Hit rate | Blocks fetched | Direct writes |
|---|---:|---:|---:|
| `--trace traces\workloads\memcpy.txt --associativity 2` | 93.75% | 2048 | 0 |
| `--trace traces\workloads\memcpy.txt --associativity 2 --no-write-allocate` | 48.44% | **512** | 16384 |

**Why:** with write-allocate, every destination block is *read* from memory
just to be overwritten, which is 4× more blocks fetched. The hit rate looks
better, but the extra traffic is wasted. For pure copying, no-write-allocate
is cheaper.

---

## Run 6: memcpy on a direct-mapped cache

| Command | Hit rate | Write-backs |
|---|---:|---:|
| `--trace traces\workloads\memcpy.txt --associativity 1` | **0.00%** | 15872 |
| `--trace traces\workloads\memcpy.txt --associativity 2` | 93.75% | 768 |

**Why:** `src[i]` and `dst[i]` map to the same set. In 1-way they keep
evicting each other: a read loads src, a write loads dst and evicts src, and
so on. 2 ways hold both.

---

## Run 7: access pattern (hardware vs software)

8-way, 32 KiB, 128×128 matrix:

| Command | Hit rate |
|---|---:|
| `--trace traces\workloads\matrix_row_major.txt --associativity 8` | 93.75% |
| `--trace traces\workloads\matrix_col_major.txt --associativity 8` | **0.00%** |
| `--trace traces\workloads\matrix_col_padded.txt --associativity 8` | 93.02% |

**Why:** row by row uses all 16 ints of each block (15 of every 16 accesses
hit). Column by column jumps 512 bytes each time, and those blocks crowd into
8 of the 64 sets, so they evict each other. Padding the rows spreads them out.

---

## Run 8: the automatic tests

VS Code: Ctrl+Shift+P, then **CMake: Run Tests**. Or in the terminal:
```
ctest --test-dir build --output-on-failure
```
Expected: `100% tests passed, 0 tests failed out of 6`.

To see each unit test by name:
```
.\build\unit_tests.exe
```
Expected: 14 lines ending in `ok`, then `49 checks, 0 failed`.

---

## Run 9: error handling

Each of these must print an error and stop:

| Command | Expected message |
|---|---|
| `--trace traces\sample_trace.txt --block-size 48` | Cache size, block size, and associativity must be powers of two. |
| `--trace traces\missing.txt` | Could not open trace file 'traces\missing.txt'. |
| `--trace traces\sample_trace.txt --policy mru` | unknown option or bad value: --policy mru |
| `--trace traces\sample_trace.txt --associativity 1024` | Associativity cannot exceed the number of cache lines. |
