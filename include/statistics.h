#ifndef CACHE_SIMULATOR_STATISTICS_H
#define CACHE_SIMULATOR_STATISTICS_H

#include <stdbool.h>
#include <stdint.h>

/** Aggregate counters collected during a simulation run. */
typedef struct Statistics {
    uint64_t total_accesses;
    uint64_t hits;
    uint64_t misses;
} Statistics;

/**
 * Resets every counter to zero.
 *
 * @param statistics Statistics object to initialize.
 */
void statistics_reset(Statistics *statistics);

/**
 * Records one cache access outcome.
 *
 * @param statistics Statistics object to update.
 * @param was_hit true for a hit, false for a miss.
 */
void statistics_record_access(Statistics *statistics, bool was_hit);

/**
 * Calculates the percentage of accesses that were cache hits.
 *
 * @param statistics Statistics to query.
 * @return Hit rate in the range 0.0 to 100.0.
 */
double statistics_hit_rate(const Statistics *statistics);

/**
 * Calculates the percentage of accesses that were cache misses.
 *
 * @param statistics Statistics to query.
 * @return Miss rate in the range 0.0 to 100.0.
 */
double statistics_miss_rate(const Statistics *statistics);

#endif
