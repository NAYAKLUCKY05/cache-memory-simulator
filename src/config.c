#include "config.h"

#include <stdio.h>

/* Version 1.0 models 32-bit byte addresses. */
#define CONFIG_V1_ADDRESS_WIDTH_BITS 32U
#define CONFIG_DEFAULT_CACHE_SIZE_BYTES (32U * 1024U)
#define CONFIG_DEFAULT_BLOCK_SIZE_BYTES 64U
#define CONFIG_DEFAULT_ASSOCIATIVITY 1U

/**
 * Writes a diagnostic only when the caller supplied usable storage.
 *
 * @param error_message Destination buffer, which may be NULL.
 * @param error_message_size Size of error_message in bytes.
 * @param message Null-terminated diagnostic message.
 */
static void config_set_error(char *error_message, size_t error_message_size,
                             const char *message)
{
    if (error_message != NULL && error_message_size > 0U) {
        (void)snprintf(error_message, error_message_size, "%s", message);
    }
}

/**
 * Tests whether a nonzero integer is a power of two.
 *
 * @param value Value to test.
 * @return true if value is a power of two; otherwise false.
 */
static bool config_is_power_of_two(size_t value)
{
    return value != 0U && (value & (value - 1U)) == 0U;
}

/**
 * Initializes a configuration with practical Version 1.0 defaults.
 *
 * @param config Configuration object to initialize. A NULL pointer is ignored.
 */
void config_set_defaults(SimulatorConfig *config)
{
    if (config == NULL) {
        return;
    }

    config->cache_size_bytes = CONFIG_DEFAULT_CACHE_SIZE_BYTES;
    config->block_size_bytes = CONFIG_DEFAULT_BLOCK_SIZE_BYTES;
    config->associativity = CONFIG_DEFAULT_ASSOCIATIVITY;
    config->address_width_bits = CONFIG_V1_ADDRESS_WIDTH_BITS;
}

/**
 * Validates cache dimensions and Version 1.0 address-width constraints.
 *
 * Cache dimensions must be positive powers of two. This ensures that cache
 * address fields can be derived solely from address bit slices.
 *
 * @param config Configuration to validate.
 * @param error_message Optional buffer that receives a human-readable error.
 * @param error_message_size Size of error_message in bytes.
 * @return true when the configuration is valid; otherwise false.
 */
bool config_validate(const SimulatorConfig *config, char *error_message,
                     size_t error_message_size)
{
    size_t line_count;

    if (config == NULL) {
        config_set_error(error_message, error_message_size,
                         "Configuration pointer must not be NULL.");
        return false;
    }

    if (config->cache_size_bytes == 0U) {
        config_set_error(error_message, error_message_size,
                         "Cache size must be greater than zero.");
        return false;
    }

    if (config->block_size_bytes == 0U) {
        config_set_error(error_message, error_message_size,
                         "Block size must be greater than zero.");
        return false;
    }

    if (config->associativity == 0U) {
        config_set_error(error_message, error_message_size,
                         "Associativity must be greater than zero.");
        return false;
    }

    if (config->address_width_bits != CONFIG_V1_ADDRESS_WIDTH_BITS) {
        config_set_error(error_message, error_message_size,
                         "Version 1.0 supports only 32-bit addresses.");
        return false;
    }

    if (!config_is_power_of_two(config->cache_size_bytes) ||
        !config_is_power_of_two(config->block_size_bytes) ||
        !config_is_power_of_two(config->associativity)) {
        config_set_error(error_message, error_message_size,
                         "Cache size, block size, and associativity must be powers of two.");
        return false;
    }

    if (config->cache_size_bytes < config->block_size_bytes ||
        config->cache_size_bytes % config->block_size_bytes != 0U) {
        config_set_error(error_message, error_message_size,
                         "Cache size must be divisible by block size.");
        return false;
    }

    line_count = config->cache_size_bytes / config->block_size_bytes;
    if (config->associativity > line_count ||
        line_count % config->associativity != 0U) {
        config_set_error(error_message, error_message_size,
                         "Associativity must divide the total number of cache lines.");
        return false;
    }

    config_set_error(error_message, error_message_size, "");
    return true;
}

/**
 * Determines the cache organization represented by a configuration.
 *
 * @param config Simulator configuration. A NULL or zero-dimension
 * configuration is treated as direct-mapped for defensive use by callers.
 * @return The corresponding cache organization.
 */
CacheOrganization config_get_organization(const SimulatorConfig *config)
{
    size_t line_count;

    if (config == NULL || config->block_size_bytes == 0U ||
        config->cache_size_bytes < config->block_size_bytes ||
        config->associativity == 0U) {
        return CACHE_ORGANIZATION_DIRECT_MAPPED;
    }

    line_count = config->cache_size_bytes / config->block_size_bytes;
    if (config->associativity == 1U) {
        return CACHE_ORGANIZATION_DIRECT_MAPPED;
    }

    if (config->associativity == line_count) {
        return CACHE_ORGANIZATION_FULLY_ASSOCIATIVE;
    }

    return CACHE_ORGANIZATION_SET_ASSOCIATIVE;
}

/**
 * Returns a printable cache organization name.
 *
 * @param organization Cache organization value.
 * @return A constant descriptive string.
 */
const char *config_organization_name(CacheOrganization organization)
{
    switch (organization) {
    case CACHE_ORGANIZATION_DIRECT_MAPPED:
        return "Direct-Mapped";
    case CACHE_ORGANIZATION_SET_ASSOCIATIVE:
        return "Set-Associative";
    case CACHE_ORGANIZATION_FULLY_ASSOCIATIVE:
        return "Fully Associative";
    default:
        return "Unknown";
    }
}
