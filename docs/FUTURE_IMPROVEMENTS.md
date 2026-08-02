# Future Improvements

Version 1.0 focuses on a transparent read-access cache model. The following
items are natural next steps, ordered roughly from local cache-engine changes
to broader simulator capabilities.

## Replacement policies

- **LRU:** track recent use per line or per set to select the least recently
  used victim.
- **FIFO:** record insertion order and evict the oldest resident line.
- **Random:** choose a victim way using a deterministic, seedable PRNG for
  reproducible experiments.
- **Policy selection:** expose replacement choice through configuration while
  keeping the access pipeline unchanged.

## Write behavior

- **Dirty bit:** add per-line dirty state for modified cache blocks.
- **Write-through:** propagate every write to backing memory.
- **Write-back:** defer memory writes until a dirty victim is evicted.
- **Write allocation policies:** model write-allocate and no-write-allocate
  behavior, with trace operations that distinguish reads and writes.

## Hierarchy and timing

- **Multi-level cache:** model L1/L2/L3 caches with independent dimensions and
  policies.
- **Latency model:** associate hit and miss penalties with each level and
  report average memory access time.
- **Prefetching:** experiment with next-line and stride prefetchers.
- **Coherence-oriented extensions:** add a foundation for multicore cache
  states and invalidation traffic.

## Tooling and usability

- **GUI or interactive visualization:** show live set contents and highlight
  decoded tag/index/offset fields for each access.
- **Structured reports:** support CSV or JSON output for experiment scripts.
- **Trace generators:** add configurable synthetic workloads for strides,
  working sets, and conflict patterns.
- **Automated unit tests:** add focused tests for decomposition, hit/miss
  cases, parser errors, and allocation failures.
- **Performance optimization:** use contiguous line storage, optional faster
  tag lookup for high associativity, and profiling-driven improvements for
  very large traces.

## Scope discipline

Each improvement should preserve the current separation between configuration,
parsing, cache behavior, statistics, and orchestration. New behavior should be
introduced through explicit configuration and tested with deterministic traces.
