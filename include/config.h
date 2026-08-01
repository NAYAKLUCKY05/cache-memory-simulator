#ifndef CACHE_SIMULATOR_CONFIG_H
#define CACHE_SIMULATOR_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Supported cache organizations derived from the cache configuration. */
typedef enum CacheOrganization {
    CACHE_ORGANIZATION_DIRECT_MAPPED,
    CACHE_ORGANIZATION_SET_ASSOCIATIVE,
    CACHE_ORGANIZATION_FULLY_ASSOCIATIVE
} CacheOrganization;

/** Immutable configuration used to create one simulation. */
typedef struct SimulatorConfig {
    size_t cache_size_bytes;
    size_t block_size_bytes;
    size_t associativity;
    uint8_t address_width_bits;
} SimulatorConfig;

/**
 * Initializes a configuration with the Version 1.0 defaults.
 *
 * @param config Configuration object to initialize.
 */
void config_set_defaults(SimulatorConfig *config);

/**
 * Validates cache dimensions and address-width constraints.
 *
 * @param config Configuration to validate.
 * @param error_message Optional buffer that receives a human-readable error.
 * @param error_message_size Size of error_message in bytes.
 * @return true when the configuration is valid; otherwise false.
 */
bool config_validate(const SimulatorConfig *config, char *error_message,
                     size_t error_message_size);

/**
 * Determines the cache organization represented by a valid configuration.
 *
 * @param config Valid simulator configuration.
 * @return The corresponding cache organization.
 */
CacheOrganization config_get_organization(const SimulatorConfig *config);

/**
 * Returns a printable cache organization name.
 *
 * @param organization Cache organization value.
 * @return A constant descriptive string.
 */
const char *config_organization_name(CacheOrganization organization);

#endif
