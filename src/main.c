#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "simulator.h"

#define ERROR_MESSAGE_SIZE 256U

/**
 * Prints command-line usage information.
 *
 * @param program_name Executable name used in the usage line.
 */
static void print_usage(const char *program_name)
{
    (void)fprintf(stderr,
                  "Usage: %s --trace <file> [--cache-size <bytes>] "
                  "[--block-size <bytes>] [--associativity <n>]\n",
                  program_name);
}

/**
 * Converts a positive decimal command-line value to size_t safely.
 *
 * @param text Text to parse.
 * @param value Destination for the parsed value.
 * @return true when text is a positive size_t value; otherwise false.
 */
static bool parse_positive_size(const char *text, size_t *value)
{
    char *end_pointer;
    unsigned long long parsed_value;

    if (text == NULL || *text == '\0' || *text == '+' || *text == '-') {
        return false;
    }

    errno = 0;
    parsed_value = strtoull(text, &end_pointer, 10);
    if (errno == ERANGE || *end_pointer != '\0' || parsed_value == 0U ||
        parsed_value > SIZE_MAX) {
        return false;
    }

    *value = (size_t)parsed_value;
    return true;
}

/**
 * Runs the cache memory simulator from command-line arguments.
 *
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument vector.
 * @return EXIT_SUCCESS on success; otherwise EXIT_FAILURE.
 */
int main(int argc, char *argv[])
{
    SimulatorConfig config;
    Simulator simulator = {0};
    const char *trace_path = NULL;
    char error_message[ERROR_MESSAGE_SIZE];
    int argument_index;

    config_set_defaults(&config);

    for (argument_index = 1; argument_index < argc; ++argument_index) {
        const char *option = argv[argument_index];
        size_t value;

        if (strcmp(option, "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }

        if (argument_index + 1 >= argc) {
            (void)fprintf(stderr, "Error: option '%s' requires a value.\n", option);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }

        if (strcmp(option, "--trace") == 0) {
            if (trace_path != NULL) {
                (void)fprintf(stderr, "Error: --trace may be specified only once.\n");
                return EXIT_FAILURE;
            }
            trace_path = argv[++argument_index];
        } else if (strcmp(option, "--cache-size") == 0) {
            if (!parse_positive_size(argv[++argument_index], &value)) {
                (void)fprintf(stderr, "Error: --cache-size must be a positive integer.\n");
                return EXIT_FAILURE;
            }
            config.cache_size_bytes = value;
        } else if (strcmp(option, "--block-size") == 0) {
            if (!parse_positive_size(argv[++argument_index], &value)) {
                (void)fprintf(stderr, "Error: --block-size must be a positive integer.\n");
                return EXIT_FAILURE;
            }
            config.block_size_bytes = value;
        } else if (strcmp(option, "--associativity") == 0) {
            if (!parse_positive_size(argv[++argument_index], &value)) {
                (void)fprintf(stderr, "Error: --associativity must be a positive integer.\n");
                return EXIT_FAILURE;
            }
            config.associativity = value;
        } else {
            (void)fprintf(stderr, "Error: unknown option '%s'.\n", option);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (trace_path == NULL) {
        (void)fprintf(stderr, "Error: a trace file is required.\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (!simulator_initialize(&simulator, &config, error_message,
                              sizeof(error_message))) {
        (void)fprintf(stderr, "Initialization error: %s\n", error_message);
        return EXIT_FAILURE;
    }

    if (!simulator_run_trace(&simulator, trace_path, error_message,
                             sizeof(error_message))) {
        (void)fprintf(stderr, "Simulation error: %s\n", error_message);
        simulator_destroy(&simulator);
        return EXIT_FAILURE;
    }

    simulator_print_report(&simulator, stdout);
    simulator_destroy(&simulator);
    return EXIT_SUCCESS;
}
