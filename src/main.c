#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "simulator.h"

#define ERROR_BUFFER_SIZE 256U

static void print_usage(const char *program)
{
    (void)fprintf(stderr,
        "Usage: %s --trace <file> [options]\n"
        "\n"
        "Cache geometry:\n"
        "  --cache-size <bytes>       total capacity (default 32768)\n"
        "  --block-size <bytes>       line size (default 64)\n"
        "  --associativity <n>        ways per set; 1 = direct-mapped (default 1)\n"
        "\n"
        "Policies:\n"
        "  --policy <lru|fifo|random> replacement policy (default lru)\n"
        "  --seed <n>                 seed for the random policy (default 1)\n"
        "  --write-policy <wb|wt>     write-back or write-through (default wb)\n"
        "  --no-write-allocate        don't load a block on a write miss\n"
        "\n"
        "Timing:\n"
        "  --hit-time <cycles>        cache hit time (default 1)\n"
        "  --miss-penalty <cycles>    extra cycles on a miss (default 100)\n"
        "\n"
        "Output:\n"
        "  --format <text|csv|json>   report format (default text)\n"
        "  --verbose                  print every access (hit/miss, evictions)\n"
        "  --help                     show this message\n",
        program);
}

static bool parse_number(const char *text, unsigned long long max, unsigned long long *out)
{
    char *end;
    unsigned long long value;

    if (text == NULL || *text == '\0' || *text == '+' || *text == '-') {
        return false;
    }

    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || value == 0U || value > max) {
        return false;
    }
    *out = value;
    return true;
}

int main(int argc, char *argv[])
{
    SimulatorConfig config;
    Simulator sim = {0};
    const char *trace_path = NULL;
    char error[ERROR_BUFFER_SIZE];
    int i;

    config_set_defaults(&config);

    for (i = 1; i < argc; ++i) {
        const char *opt = argv[i];
        const char *value;
        unsigned long long n;

        /* Flags that take no value. */
        if (strcmp(opt, "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (strcmp(opt, "--verbose") == 0) {
            config.verbose = true;
            continue;
        }
        if (strcmp(opt, "--no-write-allocate") == 0) {
            config.write_allocate = false;
            continue;
        }

        if (i + 1 >= argc) {
            (void)fprintf(stderr, "Error: option '%s' needs a value.\n", opt);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
        value = argv[++i];

        if (strcmp(opt, "--trace") == 0) {
            trace_path = value;
        } else if (strcmp(opt, "--cache-size") == 0 && parse_number(value, SIZE_MAX, &n)) {
            config.cache_size_bytes = (size_t)n;
        } else if (strcmp(opt, "--block-size") == 0 && parse_number(value, SIZE_MAX, &n)) {
            config.block_size_bytes = (size_t)n;
        } else if (strcmp(opt, "--associativity") == 0 && parse_number(value, SIZE_MAX, &n)) {
            config.associativity = (size_t)n;
        } else if (strcmp(opt, "--seed") == 0 && parse_number(value, UINT32_MAX, &n)) {
            config.random_seed = (uint32_t)n;
        } else if (strcmp(opt, "--hit-time") == 0 && parse_number(value, UINT_MAX, &n)) {
            config.hit_time_cycles = (unsigned int)n;
        } else if (strcmp(opt, "--miss-penalty") == 0 && parse_number(value, UINT_MAX, &n)) {
            config.miss_penalty_cycles = (unsigned int)n;
        } else if (strcmp(opt, "--policy") == 0 &&
                   config_parse_replacement_policy(value, &config.replacement_policy)) {
            /* parsed */
        } else if (strcmp(opt, "--write-policy") == 0 &&
                   config_parse_write_policy(value, &config.write_policy)) {
            /* parsed */
        } else if (strcmp(opt, "--format") == 0 &&
                   config_parse_output_format(value, &config.output_format)) {
            /* parsed */
        } else {
            (void)fprintf(stderr, "Error: unknown option or bad value: %s %s\n", opt, value);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (trace_path == NULL) {
        (void)fprintf(stderr, "Error: --trace is required.\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (!simulator_initialize(&sim, &config, error, sizeof(error))) {
        (void)fprintf(stderr, "Configuration error: %s\n", error);
        return EXIT_FAILURE;
    }

    if (!simulator_run_trace(&sim, trace_path, error, sizeof(error))) {
        (void)fprintf(stderr, "Trace error: %s\n", error);
        simulator_destroy(&sim);
        return EXIT_FAILURE;
    }

    simulator_print_report(&sim, stdout);
    simulator_destroy(&sim);
    return EXIT_SUCCESS;
}
