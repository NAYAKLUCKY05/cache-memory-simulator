#ifndef CACHE_SIMULATOR_SIMULATOR_H
#define CACHE_SIMULATOR_SIMULATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "cache.h"
#include "config.h"
#include "statistics.h"

/** Top-level owner that coordinates configuration, cache state, and results. */
typedef struct Simulator {
    SimulatorConfig config;
    Cache cache;
    Statistics statistics;
} Simulator;

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
                          char *error_message, size_t error_message_size);

/**
 * Releases all storage owned by a simulator.
 *
 * @param simulator Simulator object to destroy.
 */
void simulator_destroy(Simulator *simulator);

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
                         char *error_message, size_t error_message_size);

/**
 * Writes the Version 1.0 console report to a stream.
 *
 * @param simulator Completed or partially run simulator.
 * @param output Output stream that receives the report.
 */
void simulator_print_report(const Simulator *simulator, FILE *output);

#endif
