# Future Improvements

Done so far: LRU/FIFO/Random replacement, write-back and write-through,
write-allocate options, AMAT, CSV/JSON output, verbose mode, unit and
property tests, and the design experiments in RESULTS.md.

Ideas for later:

- **Multi-level cache (L1 + L2).** Misses from L1 become accesses to L2, and
  AMAT becomes `L1 hit + L1 miss rate × (L2 hit + L2 miss rate × memory)`.
- **Split I/D caches.** Separate instruction and data L1 caches, which needs
  an instruction/data marker in the trace.
- **Pseudo-LRU.** The tree-based approximation that real hardware uses, to
  compare with true LRU (experiment 3 shows why it matters).
- **Built-in sweeps.** Let one command try a list of sizes/ways/policies
  instead of running the program once per design.
- **Prefetching.** Next-line and stride prefetchers, with prefetch accuracy
  in the report.
- **Miss classification.** Separate compulsory, capacity and conflict misses
  (the "3 Cs") by also running a fully associative cache of the same size.
- **Real trace formats.** Read Valgrind Lackey output (`L`/`S`/`M` lines).
- **64-bit addresses.**
