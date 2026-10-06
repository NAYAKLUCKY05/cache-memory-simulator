#ifndef CACHE_SIMULATOR_SIMULATOR_H
#define CACHE_SIMULATOR_SIMULATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "cache.h"
#include "config.h"
#include "statistics.h"

typedef struct Simulator {
    SimulatorConfig config;
    Cache cache;
    Statistics stats;
    FILE *trace_output;   /* where --verbose writes one line per access */
} Simulator;

bool simulator_initialize(Simulator *sim, const SimulatorConfig *config,
                          char *error_message, size_t error_message_size);

void simulator_destroy(Simulator *sim);

bool simulator_run_trace(Simulator *sim, const char *trace_path,
                         char *error_message, size_t error_message_size);

/* Writes the final report in the configured format (text, CSV, or JSON). */
void simulator_print_report(const Simulator *sim, FILE *output);

#endif
