#ifndef CACHE_SIMULATOR_PARSER_H
#define CACHE_SIMULATOR_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cache.h"

/* Called once per trace entry. Return false to stop reading. */
typedef bool (*TraceCallback)(AccessType type, uint32_t address,
                              size_t line_number, void *context);

/*
 * Trace format, one access per line:
 *
 *   0x1000        read (no operation given)
 *   R 0x1000      read
 *   W 0x1000      write
 *
 * Addresses are 32-bit hex, 0x prefix optional. Blank lines and lines
 * starting with '#' are skipped.
 */
bool parser_read_trace(const char *trace_path, TraceCallback callback,
                       void *context, char *error_message,
                       size_t error_message_size);

/* Parses a single trace line. Returns false for blank/comment lines too,
 * with *is_entry set to tell the two cases apart. Exposed for unit tests. */
bool parser_parse_line(const char *line, AccessType *type, uint32_t *address,
                       bool *is_entry);

#endif
