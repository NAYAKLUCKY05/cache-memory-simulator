#include "statistics.h"

#include <stddef.h>

/**
 * Calculates the percentage represented by a counter.
 *
 * @param count Counter value to convert.
 * @param total Total number of accesses.
 * @return Percentage in the range 0.0 to 100.0, or 0.0 when total is zero.
 */
static double statistics_percentage(uint64_t count, uint64_t total)
{
    if (total == 0U) {
        return 0.0;
    }

    return ((double)count * 100.0) / (double)total;
}

/**
 * Resets every counter to zero.
 *
 * @param statistics Statistics object to initialize. A NULL pointer is ignored.
 */
void statistics_reset(Statistics *statistics)
{
    if (statistics == NULL) {
        return;
    }

    statistics->total_accesses = 0U;
    statistics->hits = 0U;
    statistics->misses = 0U;
}

/**
 * Records one cache access outcome.
 *
 * @param statistics Statistics object to update. A NULL pointer is ignored.
 * @param was_hit true for a hit, false for a miss.
 */
void statistics_record_access(Statistics *statistics, bool was_hit)
{
    if (statistics == NULL) {
        return;
    }

    ++statistics->total_accesses;
    if (was_hit) {
        ++statistics->hits;
    } else {
        ++statistics->misses;
    }
}

/**
 * Calculates the percentage of accesses that were cache hits.
 *
 * @param statistics Statistics to query.
 * @return Hit rate in the range 0.0 to 100.0, or 0.0 for a NULL pointer.
 */
double statistics_hit_rate(const Statistics *statistics)
{
    if (statistics == NULL) {
        return 0.0;
    }

    return statistics_percentage(statistics->hits, statistics->total_accesses);
}

/**
 * Calculates the percentage of accesses that were cache misses.
 *
 * @param statistics Statistics to query.
 * @return Miss rate in the range 0.0 to 100.0, or 0.0 for a NULL pointer.
 */
double statistics_miss_rate(const Statistics *statistics)
{
    if (statistics == NULL) {
        return 0.0;
    }

    return statistics_percentage(statistics->misses, statistics->total_accesses);
}
