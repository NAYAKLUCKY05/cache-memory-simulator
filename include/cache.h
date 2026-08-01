#ifndef CACHE_SIMULATOR_CACHE_H
#define CACHE_SIMULATOR_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

/** One cache line. Version 1.0 models only validity and tag state. */
typedef struct CacheLine {
    bool valid;
    uint32_t tag;
} CacheLine;

/** A cache set containing one or more ways. */
typedef struct CacheSet {
    CacheLine *lines;
} CacheSet;

/** Dynamically allocated cache state and its derived dimensions. */
typedef struct Cache {
    CacheSet *sets;
    size_t set_count;
    size_t line_count;
    size_t lines_per_set;
    uint8_t offset_bits;
    uint8_t index_bits;
} Cache;

/** Bit fields obtained from a decoded memory address. */
typedef struct AddressFields {
    uint32_t tag;
    uint32_t set_index;
    uint32_t block_offset;
} AddressFields;

/** Result of one cache lookup and possible allocation. */
typedef struct CacheAccessResult {
    AddressFields address;
    bool hit;
    size_t way_index;
} CacheAccessResult;

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
                      char *error_message, size_t error_message_size);

/**
 * Releases all storage owned by a cache. Safe to call on a zero-initialized cache.
 *
 * @param cache Cache object to destroy.
 */
void cache_destroy(Cache *cache);

/**
 * Decodes an address into tag, set index, and block offset fields.
 *
 * @param cache Initialized cache providing the derived bit widths.
 * @param address 32-bit byte address to decode.
 * @return Decoded address fields.
 */
AddressFields cache_decode_address(const Cache *cache, uint32_t address);

/**
 * Looks up an address and installs its tag on a miss.
 *
 * Empty ways are selected first. When a set is full, Version 1.0 uses the
 * lowest-index way as a deterministic placeholder until replacement policies
 * are introduced in a future version.
 *
 * @param cache Initialized cache to access.
 * @param address 32-bit byte address to access.
 * @return Hit or miss result together with decoded address fields.
 */
CacheAccessResult cache_access(Cache *cache, uint32_t address);

#endif
