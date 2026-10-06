#!/usr/bin/env python3
"""
Generates the workload traces in traces/workloads/.

Each workload is built to show one specific cache behaviour (spatial
locality, capacity misses, conflict misses, ...). The output is
deterministic (fixed random seed), so the committed traces can always be
regenerated exactly:

    python tools/gen_traces.py
"""
import os
import random

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "traces", "workloads")
KIB = 1024


def write(name, description, accesses):
    path = os.path.join(OUT_DIR, name)
    with open(path, "w", newline="\n") as f:
        for line in description.strip().splitlines():
            f.write(f"# {line}\n")
        f.write(f"# {len(accesses)} accesses\n")
        for op, addr in accesses:
            f.write(f"{op} 0x{addr:08X}\n")
    print(f"{name:28} {len(accesses):7} accesses")


def sweep(base, size, passes, step=4, op="R"):
    """Walk an array of `size` bytes, `passes` times, `step` bytes at a time."""
    return [(op, base + i) for _ in range(passes) for i in range(0, size, step)]


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    rng = random.Random(2026)

    write("cyclic_36k.txt", """
Sequential reads over 36 KiB, 4 passes: just a bit larger than a 32 KiB cache.
This is LRU's worst case - it always evicts the block that is needed next.
Random replacement keeps some of the array and does better.
""", sweep(0x10000000, 36 * KIB, 4))

    write("conflict_stride.txt", """
8 addresses exactly 32 KiB apart, read round-robin 1000 times.
In a 32 KiB direct-mapped cache they all map to the same set and
evict each other every time. With 8 or more ways they all fit.
""", [("R", 0x20000000 + k * 32 * KIB) for _ in range(1000) for k in range(8)])

    n = 128
    matrix = 0x30000000
    write("matrix_row_major.txt", """
Sum of a 128x128 int matrix (64 KiB), row by row: a[i][0], a[i][1], ...
Consecutive accesses are 4 bytes apart, so each 64-byte block serves 16 accesses.
""", [("R", matrix + (i * n + j) * 4) for i in range(n) for j in range(n)])

    write("matrix_col_major.txt", """
Same matrix, column by column: a[0][j], a[1][j], ...
Consecutive accesses are 512 bytes apart (one row), so every access is a new block.
Because 512 is a power of two, the 128 blocks of a column land in only a few
sets (64 sets direct-mapped, 32 sets 2-way, 8 sets 8-way). Each of those sets
gets more blocks than it has ways, so the column evicts itself before the next
column can reuse it. Only a fully associative cache avoids this.
""", [("R", matrix + (i * n + j) * 4) for j in range(n) for i in range(n)])

    padded = n + 1
    write("matrix_col_padded.txt", """
Column-by-column again, but each row is padded to 129 ints (516 bytes).
This is the classic software fix: the odd row size spreads a column over
many sets, so the column stays cached and the next 15 columns hit.
""", [("R", matrix + (i * padded + j) * 4) for j in range(n) for i in range(n)])

    hot_cold = []
    for _ in range(20000):
        if rng.random() < 0.9:
            hot_cold.append(("R", 0x50000000 + rng.randrange(0, 16 * KIB, 4)))
        else:
            hot_cold.append(("R", 0x60000000 + rng.randrange(0, 4096 * KIB, 4)))
    write("hot_cold.txt", """
90% of reads go to a 16 KiB "hot" region, 10% to a 4 MiB "cold" region.
This is closer to real programs. LRU keeps the hot data because it is
used constantly; FIFO and Random sometimes throw it out for cold data.
""", hot_cold)

    copy = []
    for _ in range(2):
        for i in range(0, 32 * KIB, 4):
            copy.append(("R", 0x70000000 + i))
            copy.append(("W", 0x78000000 + i))
    write("memcpy.txt", """
memcpy of 32 KiB, done twice: read src[i], write dst[i].
src and dst are 128 MiB apart, so src[i] and dst[i] map to the same set:
a direct-mapped cache ping-pongs between them (0% hits); 2 ways fix it.
Also used to compare write policies. Write-back sends each dirty block to memory
once when evicted; write-through sends every single store (16 per block).
""", copy)

    histogram = []
    for _ in range(20000):
        bucket = 0x7C000000 + rng.randrange(0, 8 * KIB, 4)
        histogram.append(("R", bucket))
        histogram.append(("W", bucket))
    write("histogram.txt", """
Histogram update: count[x]++ for 20000 random x, so read then write the same
word. The 8 KiB table fits in the cache and every word is written many times.
Write-back absorbs those repeated writes in the cache; write-through sends
all 20000 of them to memory.
""", histogram)


if __name__ == "__main__":
    main()
