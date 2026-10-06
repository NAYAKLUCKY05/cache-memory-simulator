#ifndef CACHE_SIMULATOR_STATISTICS_H
#define CACHE_SIMULATOR_STATISTICS_H

#include <stdint.h>

#include "cache.h"

typedef struct Statistics {
    uint64_t reads;
    uint64_t writes;
    uint64_t read_hits;
    uint64_t write_hits;
    uint64_t evictions;
    uint64_t writebacks;       /* dirty lines written to memory on eviction */
    uint64_t memory_writes;    /* stores sent straight to memory */
    uint64_t blocks_fetched;   /* blocks read from memory into the cache */
} Statistics;

void statistics_reset(Statistics *stats);
void statistics_record(Statistics *stats, AccessType type,
                       const CacheAccessResult *result);

uint64_t statistics_accesses(const Statistics *stats);
uint64_t statistics_hits(const Statistics *stats);
uint64_t statistics_misses(const Statistics *stats);

/* Percentages in the range 0..100. */
double statistics_hit_rate(const Statistics *stats);
double statistics_miss_rate(const Statistics *stats);

/* Average memory access time = hit time + miss rate * miss penalty. */
double statistics_amat(const Statistics *stats, unsigned int hit_time,
                       unsigned int miss_penalty);

#endif
