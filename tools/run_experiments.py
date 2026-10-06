#!/usr/bin/env python3
"""
Runs the five design experiments described in docs/RESULTS.md and writes
one CSV per experiment to docs/results/.

    python tools/run_experiments.py [path/to/cache_simulator]

The default simulator path is build/cache_simulator (or .exe on Windows).
"""
import csv
import io
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
WORKLOADS = os.path.join(ROOT, "traces", "workloads")
OUT_DIR = os.path.join(ROOT, "docs", "results")


def find_simulator():
    if len(sys.argv) > 1:
        return sys.argv[1]
    for candidate in ("build/cache_simulator", "build/cache_simulator.exe",
                      "build/Debug/cache_simulator.exe", "build/Release/cache_simulator.exe"):
        path = os.path.join(ROOT, candidate)
        if os.path.exists(path):
            return path
    sys.exit("cache_simulator not found; build it first or pass its path")


SIM = find_simulator()


def run(trace, **options):
    """Runs one simulation and returns the CSV result row as a dict."""
    args = [SIM, "--trace", os.path.join(WORKLOADS, trace), "--format", "csv"]
    for key, value in options.items():
        flag = "--" + key.replace("_", "-")
        if value is True:
            args.append(flag)
        else:
            args += [flag, str(value)]
    out = subprocess.run(args, check=True, capture_output=True, text=True).stdout
    row = next(csv.DictReader(io.StringIO(out)))
    row["trace"] = trace
    return row


def save(name, rows, columns):
    path = os.path.join(OUT_DIR, name)
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=columns, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)
    print(f"\n== {name}")
    for row in rows:
        print("  " + "  ".join(f"{c}={row[c]}" for c in columns))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    base = ["trace", "cache_size", "associativity", "replacement", "hit_rate", "amat"]

    # 1. How many ways are needed to remove conflict misses?
    rows = [run(t, cache_size=32768, associativity=a)
            for t in ("conflict_stride.txt", "memcpy.txt")
            for a in (1, 2, 4, 8, 16)]
    save("1_associativity.csv", rows, base + ["evictions"])

    # 2. How big does the cache need to be?
    rows = [run(t, cache_size=s, associativity=4)
            for t in ("hot_cold.txt", "cyclic_36k.txt")
            for s in (4096, 8192, 16384, 32768, 65536)]
    save("2_cache_size.csv", rows, base)

    # 3. Which replacement policy?
    rows = [run(t, cache_size=32768, associativity=8, policy=p)
            for t in ("hot_cold.txt", "cyclic_36k.txt")
            for p in ("lru", "fifo", "random")]
    save("3_replacement.csv", rows, base + ["evictions"])

    # 4. Which write policy? (memory traffic)
    rows = []
    for trace in ("memcpy.txt", "histogram.txt"):
        for wp in ("wb", "wt"):
            for allocate in (True, False):
                opts = {"cache_size": 32768, "associativity": 2, "write_policy": wp}
                if not allocate:
                    opts["no_write_allocate"] = True
                row = run(trace, **opts)
                # Dirty lines still in the cache at the end would be written
                # back on a flush, so count them too for a fair comparison.
                dirty_at_end = int(row["dirty_at_end"])
                row["bytes_read_from_memory"] = int(row["blocks_fetched"]) * 64
                row["bytes_written_to_memory"] = ((int(row["writebacks"]) + dirty_at_end) * 64
                                                  + int(row["memory_writes"]) * 4)
                rows.append(row)
    save("4_write_policy.csv", rows,
         ["trace", "write_policy", "write_allocate", "hit_rate", "blocks_fetched", "writebacks",
          "memory_writes", "bytes_read_from_memory", "bytes_written_to_memory"])

    # 5. Does data layout matter as much as the hardware?
    rows = [run(t, cache_size=32768, associativity=a)
            for t in ("matrix_row_major.txt", "matrix_col_major.txt", "matrix_col_padded.txt")
            for a in (1, 2, 8, 512)]
    save("5_access_pattern.csv", rows, base)


if __name__ == "__main__":
    main()
