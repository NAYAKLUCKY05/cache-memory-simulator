#ifndef CACHE_SIMULATOR_PARSER_H
#define CACHE_SIMULATOR_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Receives one validated address from a trace file. */
typedef bool (*TraceAddressCallback)(uint32_t address, size_t line_number,
                                     void *context);

/**
 * Parses a text trace and invokes a callback for each address.
 *
 * Blank lines and lines beginning with '#' are ignored. Addresses must be
 * unsigned 32-bit hexadecimal values, with an optional 0x prefix.
 *
 * @param trace_path Path to the trace file.
 * @param callback Function invoked for each parsed address.
 * @param context Caller-owned data passed to callback.
 * @param error_message Optional buffer that receives a human-readable error.
 * @param error_message_size Size of error_message in bytes.
 * @return true if the whole trace was processed; otherwise false.
 */
bool parser_read_trace(const char *trace_path, TraceAddressCallback callback,
                       void *context, char *error_message,
                       size_t error_message_size);

#endif
