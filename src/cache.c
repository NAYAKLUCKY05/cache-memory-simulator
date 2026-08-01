#include "cache.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define CACHE_V1_ADDRESS_WIDTH_BITS 32U

/**
 * Stores a formatted diagnostic when the caller supplied usable storage.
 *
 * @param error_message Destination buffer, which may be NULL.
 * @param error_message_size Size of error_message in bytes.
 * @param format printf-style diagnostic format.
 * @param ... Format arguments.
 */
static void cache_set_error(char *error_message, size_t error_message_size,
                            const char *format, ...)
{
    va_list arguments;

    if (error_message == NULL || error_message_size == 0U) {
        return;
    }

    va_start(arguments, format);
    (void)vsnprintf(error_message, error_message_size, format, arguments);
    va_end(arguments);
}

/**
 * Calculates log2 for a positive power-of-two value.
 *
 * @param value Positive power-of-two value.
 * @return Base-two logarithm of value.
 */
static uint8_t cache_log2_power_of_two(size_t value)
{
    uint8_t bit_count = 0U;

    while (value > 1U) {
        value >>= 1U;
        ++bit_count;
    }

    return bit_count;
}

/**
 * Returns a low-bit mask without shifting a 32-bit value by 32 bits.
 *
 * @param bit_count Number of low bits to retain.
 * @return Mask containing bit_count low-order one bits.
 */
static uint32_t cache_low_bit_mask(uint8_t bit_count)
{
    if (bit_count >= CACHE_V1_ADDRESS_WIDTH_BITS) {
        return UINT32_MAX;
    }

    return (UINT32_C(1) << bit_count) - UINT32_C(1);
}

/**
 * Allocates all lines in one cache set.
 *
 * @param set Cache set to initialize.
 * @param lines_per_set Number of ways in the set.
 * @return true on success; otherwise false.
 */
static bool cache_initialize_set(CacheSet *set, size_t lines_per_set)
{
    set->lines = calloc(lines_per_set, sizeof(*set->lines));
    return set->lines != NULL;
}

/**
 * Returns the matching way in a set, if present.
 *
 * @param set Cache set to search.
 * @param lines_per_set Number of ways in the set.
 * @param tag Requested address tag.
 * @return Matching way index, or lines_per_set if not found.
 */
static size_t cache_find_matching_way(const CacheSet *set, size_t lines_per_set,
                                      uint32_t tag)
{
    size_t way_index;

    for (way_index = 0U; way_index < lines_per_set; ++way_index) {
        const CacheLine *line = &set->lines[way_index];

        if (line->valid && line->tag == tag) {
            return way_index;
        }
    }

    return lines_per_set;
}

/**
 * Chooses an insertion way using Version 1.0's temporary policy.
 *
 * Invalid lines are preferred. If every way is occupied, way zero is
 * overwritten deterministically until a configurable replacement policy is
 * added in a future version.
 *
 * @param set Cache set to inspect.
 * @param lines_per_set Number of ways in the set.
 * @return Index of the selected insertion way.
 */
static size_t cache_select_insertion_way(const CacheSet *set,
                                         size_t lines_per_set)
{
    size_t way_index;

    for (way_index = 0U; way_index < lines_per_set; ++way_index) {
        if (!set->lines[way_index].valid) {
            return way_index;
        }
    }

    return 0U;
}

/**
 * Allocates and initializes cache storage for a validated configuration.
 *
 * @param cache Cache object to initialize.
 * @param config Valid simulator configuration.
 * @param error_message Optional buffer that receives a human-readable error.
 * @param error_message_size Size of error_message in bytes.
 * @return true on success; otherwise false.
 */
bool cache_initialize(Cache *cache, const SimulatorConfig *config,
                      char *error_message, size_t error_message_size)
{
    Cache initialized_cache = {0};
    size_t set_index;

    if (cache == NULL) {
        cache_set_error(error_message, error_message_size,
                        "Cache pointer must not be NULL.");
        return false;
    }

    if (!config_validate(config, error_message, error_message_size)) {
        return false;
    }

    initialized_cache.line_count =
        config->cache_size_bytes / config->block_size_bytes;
    initialized_cache.lines_per_set = config->associativity;
    initialized_cache.set_count =
        initialized_cache.line_count / initialized_cache.lines_per_set;
    initialized_cache.offset_bits =
        cache_log2_power_of_two(config->block_size_bytes);
    initialized_cache.index_bits =
        cache_log2_power_of_two(initialized_cache.set_count);

    if ((unsigned int)initialized_cache.offset_bits +
            (unsigned int)initialized_cache.index_bits >
        config->address_width_bits) {
        cache_set_error(error_message, error_message_size,
                        "Cache dimensions exceed the configured address width.");
        return false;
    }

    initialized_cache.sets =
        calloc(initialized_cache.set_count, sizeof(*initialized_cache.sets));
    if (initialized_cache.sets == NULL) {
        cache_set_error(error_message, error_message_size,
                        "Could not allocate cache sets.");
        return false;
    }

    for (set_index = 0U; set_index < initialized_cache.set_count; ++set_index) {
        if (!cache_initialize_set(&initialized_cache.sets[set_index],
                                  initialized_cache.lines_per_set)) {
            cache_set_error(error_message, error_message_size,
                            "Could not allocate cache lines.");
            cache_destroy(&initialized_cache);
            return false;
        }
    }

    *cache = initialized_cache;
    cache_set_error(error_message, error_message_size, "");
    return true;
}

/**
 * Releases all storage owned by a cache.
 *
 * @param cache Cache object to destroy. A NULL pointer is ignored.
 */
void cache_destroy(Cache *cache)
{
    size_t set_index;

    if (cache == NULL) {
        return;
    }

    if (cache->sets != NULL) {
        for (set_index = 0U; set_index < cache->set_count; ++set_index) {
            free(cache->sets[set_index].lines);
        }
    }

    free(cache->sets);
    *cache = (Cache){0};
}

/**
 * Decodes an address into tag, set index, and block offset fields.
 *
 * @param cache Initialized cache providing the derived bit widths.
 * @param address 32-bit byte address to decode.
 * @return Decoded address fields. A NULL cache produces zero fields.
 */
AddressFields cache_decode_address(const Cache *cache, uint32_t address)
{
    AddressFields fields = {0};
    uint8_t tag_shift;

    if (cache == NULL) {
        return fields;
    }

    fields.block_offset = address & cache_low_bit_mask(cache->offset_bits);
    if (cache->index_bits > 0U) {
        fields.set_index = (address >> cache->offset_bits) &
                           cache_low_bit_mask(cache->index_bits);
    }

    tag_shift = (uint8_t)(cache->offset_bits + cache->index_bits);
    if (tag_shift < CACHE_V1_ADDRESS_WIDTH_BITS) {
        fields.tag = address >> tag_shift;
    }

    return fields;
}

/**
 * Looks up an address and installs its tag on a miss.
 *
 * The same set-based implementation serves every Version 1.0 organization:
 * direct-mapped caches have one way per set, set-associative caches have
 * multiple ways per set, and fully associative caches have one set.
 *
 * @param cache Initialized cache to access.
 * @param address 32-bit byte address to access.
 * @return Hit or miss result together with decoded address fields.
 */
CacheAccessResult cache_access(Cache *cache, uint32_t address)
{
    CacheAccessResult result = {0};
    CacheSet *set;
    size_t way_index;

    if (cache == NULL || cache->sets == NULL || cache->set_count == 0U ||
        cache->lines_per_set == 0U) {
        return result;
    }

    result.address = cache_decode_address(cache, address);
    if (result.address.set_index >= cache->set_count) {
        return result;
    }

    set = &cache->sets[result.address.set_index];
    if (set->lines == NULL) {
        return result;
    }

    way_index = cache_find_matching_way(set, cache->lines_per_set,
                                        result.address.tag);
    if (way_index < cache->lines_per_set) {
        result.hit = true;
        result.way_index = way_index;
        return result;
    }

    way_index = cache_select_insertion_way(set, cache->lines_per_set);
    set->lines[way_index].tag = result.address.tag;
    set->lines[way_index].valid = true;
    result.way_index = way_index;
    return result;
}
