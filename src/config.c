#include "config.h"

#include <ctype.h>
#include <stdio.h>

#define ADDRESS_WIDTH_BITS 32U

#define DEFAULT_CACHE_SIZE (32U * 1024U)
#define DEFAULT_BLOCK_SIZE 64U
#define DEFAULT_ASSOCIATIVITY 1U
#define DEFAULT_RANDOM_SEED 1U
#define DEFAULT_HIT_TIME 1U
#define DEFAULT_MISS_PENALTY 100U

static void set_error(char *buffer, size_t size, const char *message)
{
    if (buffer != NULL && size > 0U) {
        (void)snprintf(buffer, size, "%s", message);
    }
}

static bool is_power_of_two(size_t value)
{
    return value != 0U && (value & (value - 1U)) == 0U;
}

static bool equals_ignore_case(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == *b;
}

void config_set_defaults(SimulatorConfig *config)
{
    if (config == NULL) {
        return;
    }

    config->cache_size_bytes = DEFAULT_CACHE_SIZE;
    config->block_size_bytes = DEFAULT_BLOCK_SIZE;
    config->associativity = DEFAULT_ASSOCIATIVITY;
    config->address_width_bits = ADDRESS_WIDTH_BITS;
    config->replacement_policy = REPLACEMENT_POLICY_LRU;
    config->random_seed = DEFAULT_RANDOM_SEED;
    config->write_policy = WRITE_POLICY_WRITE_BACK;
    config->write_allocate = true;
    config->hit_time_cycles = DEFAULT_HIT_TIME;
    config->miss_penalty_cycles = DEFAULT_MISS_PENALTY;
    config->output_format = OUTPUT_FORMAT_TEXT;
    config->verbose = false;
}

bool config_validate(const SimulatorConfig *config, char *error_message,
                     size_t error_message_size)
{
    size_t line_count;

    if (config == NULL) {
        set_error(error_message, error_message_size,
                  "Configuration pointer must not be NULL.");
        return false;
    }

    if (config->cache_size_bytes == 0U || config->block_size_bytes == 0U ||
        config->associativity == 0U) {
        set_error(error_message, error_message_size,
                  "Cache size, block size, and associativity must be greater than zero.");
        return false;
    }

    if (config->address_width_bits != ADDRESS_WIDTH_BITS) {
        set_error(error_message, error_message_size,
                  "Only 32-bit addresses are supported.");
        return false;
    }

    if (!is_power_of_two(config->cache_size_bytes) ||
        !is_power_of_two(config->block_size_bytes) ||
        !is_power_of_two(config->associativity)) {
        set_error(error_message, error_message_size,
                  "Cache size, block size, and associativity must be powers of two.");
        return false;
    }

    if (config->cache_size_bytes < config->block_size_bytes) {
        set_error(error_message, error_message_size,
                  "Cache size must be at least one block.");
        return false;
    }

    line_count = config->cache_size_bytes / config->block_size_bytes;
    if (config->associativity > line_count) {
        set_error(error_message, error_message_size,
                  "Associativity cannot exceed the number of cache lines.");
        return false;
    }

    if (config->replacement_policy > REPLACEMENT_POLICY_RANDOM ||
        config->write_policy > WRITE_POLICY_WRITE_THROUGH ||
        config->output_format > OUTPUT_FORMAT_JSON) {
        set_error(error_message, error_message_size,
                  "Unknown replacement policy, write policy, or output format.");
        return false;
    }

    if (config->hit_time_cycles == 0U) {
        set_error(error_message, error_message_size,
                  "Hit time must be at least one cycle.");
        return false;
    }

    set_error(error_message, error_message_size, "");
    return true;
}

CacheOrganization config_get_organization(const SimulatorConfig *config)
{
    size_t line_count;

    if (config == NULL || config->block_size_bytes == 0U ||
        config->cache_size_bytes < config->block_size_bytes ||
        config->associativity <= 1U) {
        return CACHE_ORGANIZATION_DIRECT_MAPPED;
    }

    line_count = config->cache_size_bytes / config->block_size_bytes;
    if (config->associativity == line_count) {
        return CACHE_ORGANIZATION_FULLY_ASSOCIATIVE;
    }
    return CACHE_ORGANIZATION_SET_ASSOCIATIVE;
}

const char *config_organization_name(CacheOrganization organization)
{
    switch (organization) {
    case CACHE_ORGANIZATION_DIRECT_MAPPED:
        return "Direct-Mapped";
    case CACHE_ORGANIZATION_SET_ASSOCIATIVE:
        return "Set-Associative";
    case CACHE_ORGANIZATION_FULLY_ASSOCIATIVE:
        return "Fully Associative";
    }
    return "Unknown";
}

const char *config_replacement_policy_name(ReplacementPolicy policy)
{
    switch (policy) {
    case REPLACEMENT_POLICY_LRU:
        return "LRU";
    case REPLACEMENT_POLICY_FIFO:
        return "FIFO";
    case REPLACEMENT_POLICY_RANDOM:
        return "Random";
    }
    return "Unknown";
}

const char *config_write_policy_name(WritePolicy policy)
{
    switch (policy) {
    case WRITE_POLICY_WRITE_BACK:
        return "Write-Back";
    case WRITE_POLICY_WRITE_THROUGH:
        return "Write-Through";
    }
    return "Unknown";
}

bool config_parse_replacement_policy(const char *text,
                                     ReplacementPolicy *policy)
{
    if (text == NULL || policy == NULL) {
        return false;
    }

    if (equals_ignore_case(text, "lru")) {
        *policy = REPLACEMENT_POLICY_LRU;
    } else if (equals_ignore_case(text, "fifo")) {
        *policy = REPLACEMENT_POLICY_FIFO;
    } else if (equals_ignore_case(text, "random")) {
        *policy = REPLACEMENT_POLICY_RANDOM;
    } else {
        return false;
    }
    return true;
}

bool config_parse_write_policy(const char *text, WritePolicy *policy)
{
    if (text == NULL || policy == NULL) {
        return false;
    }

    if (equals_ignore_case(text, "write-back") || equals_ignore_case(text, "wb")) {
        *policy = WRITE_POLICY_WRITE_BACK;
    } else if (equals_ignore_case(text, "write-through") ||
               equals_ignore_case(text, "wt")) {
        *policy = WRITE_POLICY_WRITE_THROUGH;
    } else {
        return false;
    }
    return true;
}

bool config_parse_output_format(const char *text, OutputFormat *format)
{
    if (text == NULL || format == NULL) {
        return false;
    }

    if (equals_ignore_case(text, "text")) {
        *format = OUTPUT_FORMAT_TEXT;
    } else if (equals_ignore_case(text, "csv")) {
        *format = OUTPUT_FORMAT_CSV;
    } else if (equals_ignore_case(text, "json")) {
        *format = OUTPUT_FORMAT_JSON;
    } else {
        return false;
    }
    return true;
}
