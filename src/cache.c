#include "cache.h"

#include <stdio.h>
#include <stdlib.h>

static void set_error(char *buffer, size_t size, const char *message)
{
    if (buffer != NULL && size > 0U) {
        (void)snprintf(buffer, size, "%s", message);
    }
}

static uint8_t log2_of(size_t power_of_two)
{
    uint8_t bits = 0U;

    while (power_of_two > 1U) {
        power_of_two >>= 1U;
        ++bits;
    }
    return bits;
}

static uint32_t low_mask(uint8_t bits)
{
    return bits >= 32U ? UINT32_MAX : (UINT32_C(1) << bits) - 1U;
}

/* xorshift32: tiny, fast, and gives the same sequence on every platform. */
static uint32_t next_random(uint32_t *state)
{
    uint32_t x = *state;

    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    *state = x;
    return x;
}

static CacheLine *set_lines(const Cache *cache, uint32_t set_index)
{
    return &cache->lines[(size_t)set_index * cache->ways];
}

static size_t find_way(const CacheLine *set, size_t ways, uint32_t tag)
{
    size_t way;

    for (way = 0U; way < ways; ++way) {
        if (set[way].valid && set[way].tag == tag) {
            return way;
        }
    }
    return ways;
}

static size_t choose_victim(Cache *cache, const CacheLine *set)
{
    size_t way;
    size_t victim = 0U;

    /* An empty line is always the best choice. */
    for (way = 0U; way < cache->ways; ++way) {
        if (!set[way].valid) {
            return way;
        }
    }

    switch (cache->replacement_policy) {
    case REPLACEMENT_POLICY_RANDOM:
        return (size_t)(next_random(&cache->rng_state) % (uint32_t)cache->ways);

    case REPLACEMENT_POLICY_FIFO:
        for (way = 1U; way < cache->ways; ++way) {
            if (set[way].loaded_at < set[victim].loaded_at) {
                victim = way;
            }
        }
        return victim;

    case REPLACEMENT_POLICY_LRU:
    default:
        for (way = 1U; way < cache->ways; ++way) {
            if (set[way].last_used < set[victim].last_used) {
                victim = way;
            }
        }
        return victim;
    }
}

bool cache_initialize(Cache *cache, const SimulatorConfig *config,
                      char *error_message, size_t error_message_size)
{
    size_t line_count;

    if (cache == NULL) {
        set_error(error_message, error_message_size, "Cache pointer must not be NULL.");
        return false;
    }

    if (!config_validate(config, error_message, error_message_size)) {
        return false;
    }

    *cache = (Cache){0};
    line_count = config->cache_size_bytes / config->block_size_bytes;
    cache->ways = config->associativity;
    cache->set_count = line_count / cache->ways;
    cache->offset_bits = log2_of(config->block_size_bytes);
    cache->index_bits = log2_of(cache->set_count);

    if ((unsigned int)cache->offset_bits + cache->index_bits > config->address_width_bits) {
        set_error(error_message, error_message_size,
                  "Cache dimensions exceed the address width.");
        return false;
    }

    cache->replacement_policy = config->replacement_policy;
    cache->write_policy = config->write_policy;
    cache->write_allocate = config->write_allocate;
    /* xorshift gets stuck at zero, so never start there. */
    cache->rng_state = config->random_seed != 0U ? config->random_seed : 1U;

    cache->lines = calloc(line_count, sizeof(*cache->lines));
    if (cache->lines == NULL) {
        set_error(error_message, error_message_size, "Could not allocate cache lines.");
        return false;
    }

    set_error(error_message, error_message_size, "");
    return true;
}

void cache_destroy(Cache *cache)
{
    if (cache == NULL) {
        return;
    }
    free(cache->lines);
    *cache = (Cache){0};
}

AddressFields cache_decode_address(const Cache *cache, uint32_t address)
{
    AddressFields fields = {0};
    unsigned int tag_shift;

    if (cache == NULL) {
        return fields;
    }

    fields.block_offset = address & low_mask(cache->offset_bits);
    fields.set_index = (address >> cache->offset_bits) & low_mask(cache->index_bits);

    tag_shift = (unsigned int)cache->offset_bits + cache->index_bits;
    fields.tag = tag_shift >= 32U ? 0U : address >> tag_shift;
    return fields;
}

CacheAccessResult cache_access(Cache *cache, AccessType type, uint32_t address)
{
    CacheAccessResult result = {0};
    CacheLine *set;
    CacheLine *line;
    bool is_write = type == ACCESS_WRITE;

    if (cache == NULL || cache->lines == NULL) {
        return result;
    }

    ++cache->clock;
    result.address = cache_decode_address(cache, address);
    set = set_lines(cache, result.address.set_index);
    result.way = find_way(set, cache->ways, result.address.tag);

    if (result.way < cache->ways) {
        result.hit = true;
        line = &set[result.way];
        line->last_used = cache->clock;

        if (is_write) {
            if (cache->write_policy == WRITE_POLICY_WRITE_THROUGH) {
                result.memory_write = true;
            } else {
                line->dirty = true;
            }
        }
        return result;
    }

    /* Miss. A store with no-write-allocate goes to memory and skips the cache. */
    if (is_write && !cache->write_allocate) {
        result.memory_write = true;
        return result;
    }

    result.way = choose_victim(cache, set);
    line = &set[result.way];

    if (line->valid) {
        result.evicted = true;
        result.evicted_tag = line->tag;
        result.writeback = line->dirty;
    }

    line->valid = true;
    line->dirty = false;
    line->tag = result.address.tag;
    line->loaded_at = cache->clock;
    line->last_used = cache->clock;
    result.allocated = true;

    if (is_write) {
        if (cache->write_policy == WRITE_POLICY_WRITE_THROUGH) {
            result.memory_write = true;
        } else {
            line->dirty = true;
        }
    }
    return result;
}

size_t cache_count_dirty(const Cache *cache)
{
    size_t i;
    size_t dirty = 0U;

    if (cache == NULL || cache->lines == NULL) {
        return 0U;
    }

    for (i = 0U; i < cache->set_count * cache->ways; ++i) {
        if (cache->lines[i].valid && cache->lines[i].dirty) {
            ++dirty;
        }
    }
    return dirty;
}
