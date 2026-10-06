#include "statistics.h"

#include <stddef.h>

static double percent(uint64_t part, uint64_t total)
{
    return total == 0U ? 0.0 : (double)part * 100.0 / (double)total;
}

void statistics_reset(Statistics *stats)
{
    if (stats != NULL) {
        *stats = (Statistics){0};
    }
}

void statistics_record(Statistics *stats, AccessType type,
                       const CacheAccessResult *result)
{
    if (stats == NULL || result == NULL) {
        return;
    }

    if (type == ACCESS_WRITE) {
        ++stats->writes;
        stats->write_hits += result->hit ? 1U : 0U;
    } else {
        ++stats->reads;
        stats->read_hits += result->hit ? 1U : 0U;
    }

    stats->evictions += result->evicted ? 1U : 0U;
    stats->writebacks += result->writeback ? 1U : 0U;
    stats->memory_writes += result->memory_write ? 1U : 0U;
    stats->blocks_fetched += result->allocated ? 1U : 0U;
}

uint64_t statistics_accesses(const Statistics *stats)
{
    return stats == NULL ? 0U : stats->reads + stats->writes;
}

uint64_t statistics_hits(const Statistics *stats)
{
    return stats == NULL ? 0U : stats->read_hits + stats->write_hits;
}

uint64_t statistics_misses(const Statistics *stats)
{
    return statistics_accesses(stats) - statistics_hits(stats);
}

double statistics_hit_rate(const Statistics *stats)
{
    return percent(statistics_hits(stats), statistics_accesses(stats));
}

double statistics_miss_rate(const Statistics *stats)
{
    return percent(statistics_misses(stats), statistics_accesses(stats));
}

double statistics_amat(const Statistics *stats, unsigned int hit_time,
                       unsigned int miss_penalty)
{
    return (double)hit_time +
           statistics_miss_rate(stats) / 100.0 * (double)miss_penalty;
}
