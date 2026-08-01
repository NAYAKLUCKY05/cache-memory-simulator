#include "simulator.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

#include "parser.h"

/**
 * Stores a formatted diagnostic when the caller supplied usable storage.
 *
 * @param error_message Destination buffer, which may be NULL.
 * @param error_message_size Size of error_message in bytes.
 * @param format printf-style diagnostic format.
 * @param ... Format arguments.
 */
static void simulator_set_error(char *error_message, size_t error_message_size,
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
 * Receives a parsed address and coordinates its cache access and accounting.
 *
 * @param address Parsed 32-bit byte address.
 * @param line_number Source line number, unused by the simulation engine.
 * @param context Simulator receiving the access.
 * @return true when the access was recorded; otherwise false.
 */
static bool simulator_process_address(uint32_t address, size_t line_number,
                                      void *context)
{
    Simulator *simulator = context;
    CacheAccessResult access_result;

    (void)line_number;
    if (simulator == NULL) {
        return false;
    }

    access_result = cache_access(&simulator->cache, address);
    statistics_record_access(&simulator->statistics, access_result.hit);
    return true;
}

/**
 * Tests whether the simulator owns an initialized cache.
 *
 * @param simulator Simulator to inspect.
 * @return true when the cache has usable set storage; otherwise false.
 */
static bool simulator_is_initialized(const Simulator *simulator)
{
    return simulator != NULL && simulator->cache.sets != NULL &&
           simulator->cache.set_count > 0U &&
           simulator->cache.lines_per_set > 0U;
}

/**
 * Initializes a simulator from the requested cache configuration.
 *
 * @param simulator Simulator object to initialize.
 * @param config Requested cache configuration.
 * @param error_message Optional buffer that receives a human-readable error.
 * @param error_message_size Size of error_message in bytes.
 * @return true on success; otherwise false.
 */
bool simulator_initialize(Simulator *simulator, const SimulatorConfig *config,
                          char *error_message, size_t error_message_size)
{
    if (simulator == NULL) {
        simulator_set_error(error_message, error_message_size,
                            "Simulator pointer must not be NULL.");
        return false;
    }

    if (config == NULL) {
        simulator_set_error(error_message, error_message_size,
                            "Simulator configuration must not be NULL.");
        return false;
    }

    if (!cache_initialize(&simulator->cache, config, error_message,
                          error_message_size)) {
        return false;
    }

    simulator->config = *config;
    statistics_reset(&simulator->statistics);
    simulator_set_error(error_message, error_message_size, "");
    return true;
}

/**
 * Releases all storage owned by a simulator.
 *
 * @param simulator Simulator object to destroy. A NULL pointer is ignored.
 */
void simulator_destroy(Simulator *simulator)
{
    if (simulator == NULL) {
        return;
    }

    cache_destroy(&simulator->cache);
    statistics_reset(&simulator->statistics);
    simulator->config = (SimulatorConfig){0};
}

/**
 * Runs a memory trace through an initialized simulator.
 *
 * @param simulator Initialized simulator.
 * @param trace_path Path to a memory trace file.
 * @param error_message Optional buffer that receives a human-readable error.
 * @param error_message_size Size of error_message in bytes.
 * @return true when the trace is processed completely; otherwise false.
 */
bool simulator_run_trace(Simulator *simulator, const char *trace_path,
                         char *error_message, size_t error_message_size)
{
    if (!simulator_is_initialized(simulator)) {
        simulator_set_error(error_message, error_message_size,
                            "Simulator has not been initialized.");
        return false;
    }

    statistics_reset(&simulator->statistics);
    return parser_read_trace(trace_path, simulator_process_address, simulator,
                             error_message, error_message_size);
}

/**
 * Writes the Version 1.0 console report to a stream.
 *
 * @param simulator Completed or partially run simulator.
 * @param output Output stream that receives the report. NULL is ignored.
 */
void simulator_print_report(const Simulator *simulator, FILE *output)
{
    if (simulator == NULL || output == NULL) {
        return;
    }

    (void)fprintf(output,
                  "\n"
                  "Cache Memory Simulator Report\n"
                  "=============================\n"
                  "Cache type:      %s\n"
                  "Cache size:      %zu bytes\n"
                  "Block size:      %zu bytes\n"
                  "Associativity:   %zu\n"
                  "Address width:   %u bits\n"
                  "Total accesses:  %" PRIu64 "\n"
                  "Hits:            %" PRIu64 "\n"
                  "Misses:          %" PRIu64 "\n"
                  "Hit rate:        %.2f%%\n"
                  "Miss rate:       %.2f%%\n",
                  config_organization_name(
                      config_get_organization(&simulator->config)),
                  simulator->config.cache_size_bytes,
                  simulator->config.block_size_bytes,
                  simulator->config.associativity,
                  (unsigned int)simulator->config.address_width_bits,
                  simulator->statistics.total_accesses, simulator->statistics.hits,
                  simulator->statistics.misses,
                  statistics_hit_rate(&simulator->statistics),
                  statistics_miss_rate(&simulator->statistics));
}
