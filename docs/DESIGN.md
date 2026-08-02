# Design Notes

## Modular architecture

The simulator uses modules with narrow responsibilities rather than a single
large program. Cache modeling changes are therefore local to `cache.c`, trace
format changes are local to `parser.c`, and presentation changes are local to
the report function in `simulator.c`. This is easier to review, test, and
extend than coupling command-line parsing to cache internals.

Public headers describe the contract between modules. Implementations retain
their helper functions as `static`, avoiding accidental global coupling.

## Dynamic cache allocation

Cache size, block size, and associativity are command-line configurable. A
fixed compile-time cache array would either impose an artificial maximum or
waste memory. Instead, the cache derives line count and set count at runtime,
then allocates exactly the required hierarchy of sets and lines.

Dynamic storage also makes the ownership model explicit: `cache_initialize`
creates the resources and `cache_destroy` releases them. `Simulator` owns the
cache for the duration of a simulation.

## Generic cache access path

Direct-mapped, set-associative, and fully associative caches differ in set and
way counts, not in the fundamental lookup operation. The implementation uses
one algorithm:

1. Decode tag, index, and offset.
2. Locate the indexed set.
3. Search its ways for a valid matching tag.
4. On a miss, install into an invalid way or use the temporary victim.

This reduces duplicate code and makes replacement policy a focused future
extension. A direct-mapped cache simply has one way per set; a fully
associative cache has one set.

## Error handling

The public APIs return `bool` for operations that can fail and accept an
optional caller-provided error buffer. This keeps diagnostics near their source
without requiring global error state or dynamic error allocation.

The project validates:

- null pointers at module boundaries;
- zero, incompatible, and non-power-of-two cache dimensions;
- unsupported address widths;
- failed cache allocations;
- missing trace files, malformed addresses, and read/close failures;
- invalid command-line option values.

Cleanup is performed on partial cache allocation, failed simulations, and the
normal program exit path.

## Numerical behavior

Counters are 64-bit unsigned integers. Hit and miss rates are calculated using
`double` conversion before division, avoiding integer truncation. A run with
zero accesses reports `0.0%` for both rates.

## Extensibility

Version 1.0 deliberately models only the valid bit and tag in each cache line.
Future metadata such as dirty bits, replacement timestamps, or FIFO sequence
numbers can be added to `CacheLine` without changing the simulator's orchestration.
Likewise, the deterministic victim selection is isolated in the cache engine,
where it can be replaced by a policy abstraction without changing the parser,
statistics, or CLI modules.
