#ifndef CACHE_SIMULATOR_CONFIG_H
#define CACHE_SIMULATOR_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum CacheOrganization {
    CACHE_ORGANIZATION_DIRECT_MAPPED,
    CACHE_ORGANIZATION_SET_ASSOCIATIVE,
    CACHE_ORGANIZATION_FULLY_ASSOCIATIVE
} CacheOrganization;

/* Which line to throw out when a set is full. */
typedef enum ReplacementPolicy {
    REPLACEMENT_POLICY_LRU,
    REPLACEMENT_POLICY_FIFO,
    REPLACEMENT_POLICY_RANDOM
} ReplacementPolicy;

/* When a store reaches main memory. */
typedef enum WritePolicy {
    WRITE_POLICY_WRITE_BACK,
    WRITE_POLICY_WRITE_THROUGH
} WritePolicy;

typedef enum OutputFormat {
    OUTPUT_FORMAT_TEXT,
    OUTPUT_FORMAT_CSV,
    OUTPUT_FORMAT_JSON
} OutputFormat;

typedef struct SimulatorConfig {
    size_t cache_size_bytes;
    size_t block_size_bytes;
    size_t associativity;
    uint8_t address_width_bits;

    ReplacementPolicy replacement_policy;
    uint32_t random_seed;

    WritePolicy write_policy;
    bool write_allocate;

    /* Timing model, in CPU cycles. */
    unsigned int hit_time_cycles;
    unsigned int miss_penalty_cycles;

    OutputFormat output_format;
    bool verbose;
} SimulatorConfig;

void config_set_defaults(SimulatorConfig *config);

/* Returns false and fills error_message if the configuration is unusable. */
bool config_validate(const SimulatorConfig *config, char *error_message,
                     size_t error_message_size);

CacheOrganization config_get_organization(const SimulatorConfig *config);

const char *config_organization_name(CacheOrganization organization);
const char *config_replacement_policy_name(ReplacementPolicy policy);
const char *config_write_policy_name(WritePolicy policy);

/* Name parsers used by the command line. Matching ignores case. */
bool config_parse_replacement_policy(const char *text,
                                     ReplacementPolicy *policy);
bool config_parse_write_policy(const char *text, WritePolicy *policy);
bool config_parse_output_format(const char *text, OutputFormat *format);

#endif
