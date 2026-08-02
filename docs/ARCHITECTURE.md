# Architecture

## Overview

The simulator is organized as a small pipeline. Each module owns one concern
and communicates through narrow C interfaces in `include/`. The simulator
module coordinates work; it does not implement cache placement or replacement
logic itself.

```text
CLI (main)
  │ builds SimulatorConfig
  ▼
Simulation engine (simulator)
  ├── validates/allocates through Cache
  ├── streams trace addresses through Parser
  ├── sends each address to Cache
  └── records each outcome through Statistics
```

## Module interactions

| Module | Responsibility | Collaborates with |
|---|---|---|
| `main` | Parses command-line options and owns program lifetime. | `config`, `simulator` |
| `config` | Supplies defaults, validates dimensions, identifies organization. | `cache`, `simulator`, `main` |
| `parser` | Opens, validates, and streams trace-file addresses. | `simulator` callback |
| `cache` | Allocates cache state, decodes addresses, and performs lookups. | `config` |
| `statistics` | Counts accesses, hits, misses, and percentage rates. | `simulator` |
| `simulator` | Orchestrates the complete run and writes the final report. | all runtime modules |

## Data structures

`SimulatorConfig` describes the requested cache: byte capacity, block size,
associativity, and address width.

```text
Cache
├── set_count
├── line_count
├── lines_per_set
├── offset_bits / index_bits
└── sets[]
    ├── set[0].lines[] -> CacheLine { valid, tag }
    ├── set[1].lines[] -> CacheLine { valid, tag }
    └── ...
```

`Statistics` holds 64-bit counters for total accesses, hits, and misses.
`Simulator` owns one configuration, one cache, and one statistics object for a
simulation run.

## Address decomposition

For a byte address, the cache derives three fields:

```text
31                         tag        index          offset 0
+--------------------------+------------+-------------+
|        tag bits           | index bits | offset bits |
+--------------------------+------------+-------------+
```

- **Offset:** selects a byte inside the cache block; its width is `log2(block
  size)`.
- **Index:** selects the set; its width is `log2(number of sets)`.
- **Tag:** identifies which memory block currently occupies a line in the set.

The configuration requires power-of-two dimensions so these fields can be
obtained with shifts and masks. Version 1.0 accepts unsigned 32-bit addresses.

## Cache organization

The engine uses one generic set lookup, with layout determining behavior.

| Organization | Sets | Ways per set |
|---|---:|---:|
| Direct-mapped | number of lines | 1 |
| Set-associative | line count / associativity | associativity |
| Fully associative | 1 | number of lines |

For every access, the cache decodes the address, selects its set, and scans
the set's ways for a valid matching tag. A match is a hit. On a miss, the first
invalid way is used. If a set is full, Version 1.0 replaces way 0
deterministically; this is intentionally a temporary replacement mechanism.

## Memory allocation strategy

Cache allocation is two-level:

1. Allocate the contiguous array of `CacheSet` objects.
2. Allocate a contiguous `CacheLine` array for each set.

`calloc` initializes every valid bit and tag to zero. If a line-array
allocation fails, the cache destructor releases all allocations completed so
far before initialization returns failure. Destruction walks the set array,
frees each line array, frees the set array, and zeroes the cache object.

The parser is streaming: it has a fixed line buffer and invokes a callback per
address, so trace length does not determine heap usage.
