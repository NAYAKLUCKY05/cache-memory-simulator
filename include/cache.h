#ifndef CACHE_SIMULATOR_CACHE_H
#define CACHE_SIMULATOR_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

typedef enum AccessType {
    ACCESS_READ,
    ACCESS_WRITE
} AccessType;

typedef struct CacheLine {
    bool valid;
    bool dirty;
    uint32_t tag;
    uint64_t last_used;   /* for LRU */
    uint64_t loaded_at;   /* for FIFO */
} CacheLine;

/*
 * All lines live in one array; set s owns lines[s * ways .. s * ways + ways - 1].
 * Direct-mapped is ways == 1, fully associative is set_count == 1.
 */
typedef struct Cache {
    CacheLine *lines;
    size_t set_count;
    size_t ways;
    uint8_t offset_bits;
    uint8_t index_bits;

    ReplacementPolicy replacement_policy;
    WritePolicy write_policy;
    bool write_allocate;

    uint64_t clock;
    uint32_t rng_state;
} Cache;

typedef struct AddressFields {
    uint32_t tag;
    uint32_t set_index;
    uint32_t block_offset;
} AddressFields;

typedef struct CacheAccessResult {
    AddressFields address;
    bool hit;
    bool allocated;        /* the block was brought into the cache */
    bool evicted;          /* a valid line was replaced */
    uint32_t evicted_tag;
    bool writeback;        /* the evicted line was dirty and went to memory */
    bool memory_write;     /* this store went straight to memory */
    size_t way;
} CacheAccessResult;

bool cache_initialize(Cache *cache, const SimulatorConfig *config,
                      char *error_message, size_t error_message_size);

/* Safe on a zeroed or already-destroyed cache. */
void cache_destroy(Cache *cache);

AddressFields cache_decode_address(const Cache *cache, uint32_t address);

/* Looks up one address, updating cache state for the given access type. */
CacheAccessResult cache_access(Cache *cache, AccessType type, uint32_t address);

/* Number of dirty lines still in the cache (they would be written back on flush). */
size_t cache_count_dirty(const Cache *cache);

#endif
