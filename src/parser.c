#include "parser.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PARSER_LINE_BUFFER_SIZE 256U

/**
 * Writes a formatted diagnostic when the caller supplied usable storage.
 *
 * @param error_message Destination buffer, which may be NULL.
 * @param error_message_size Size of error_message in bytes.
 * @param format printf-style diagnostic format.
 * @param ... Format arguments.
 */
static void parser_set_error(char *error_message, size_t error_message_size,
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
 * Advances a pointer past leading whitespace characters.
 *
 * @param text Text to inspect.
 * @return Pointer to the first non-whitespace character.
 */
static const char *parser_skip_whitespace(const char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text) != 0) {
        ++text;
    }

    return text;
}

/**
 * Consumes the remaining characters in an overlong trace line.
 *
 * @param trace Open trace stream.
 * @return true if the remainder was consumed without an I/O error.
 */
static bool parser_discard_line_remainder(FILE *trace)
{
    int character;

    do {
        character = fgetc(trace);
    } while (character != '\n' && character != EOF);

    return ferror(trace) == 0;
}

/**
 * Parses a text trace and invokes a callback for each address.
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
                       size_t error_message_size)
{
    char line_buffer[PARSER_LINE_BUFFER_SIZE];
    FILE *trace;
    size_t line_number = 0U;

    if (trace_path == NULL || trace_path[0] == '\0') {
        parser_set_error(error_message, error_message_size,
                         "Trace path must not be empty.");
        return false;
    }

    if (callback == NULL) {
        parser_set_error(error_message, error_message_size,
                         "Trace address callback must not be NULL.");
        return false;
    }

    trace = fopen(trace_path, "r");
    if (trace == NULL) {
        parser_set_error(error_message, error_message_size,
                         "Could not open trace file '%s'.", trace_path);
        return false;
    }

    while (fgets(line_buffer, sizeof(line_buffer), trace) != NULL) {
        const char *address_text;
        char *end_pointer;
        unsigned long long parsed_address;
        bool line_is_complete;

        ++line_number;
        address_text = parser_skip_whitespace(line_buffer);
        line_is_complete = strchr(line_buffer, '\n') != NULL || feof(trace);

        if (*address_text == '#' || *address_text == '\0') {
            if (!line_is_complete && !parser_discard_line_remainder(trace)) {
                parser_set_error(error_message, error_message_size,
                                 "I/O error while reading trace file.");
                (void)fclose(trace);
                return false;
            }
            continue;
        }

        if (!line_is_complete) {
            if (!parser_discard_line_remainder(trace)) {
                parser_set_error(error_message, error_message_size,
                                 "I/O error while reading trace file.");
            } else {
                parser_set_error(error_message, error_message_size,
                                 "Trace line %zu exceeds %u characters.", line_number,
                                 PARSER_LINE_BUFFER_SIZE - 1U);
            }
            (void)fclose(trace);
            return false;
        }

        if (*address_text == '+' || *address_text == '-') {
            parser_set_error(error_message, error_message_size,
                             "Invalid address on trace line %zu.", line_number);
            (void)fclose(trace);
            return false;
        }

        errno = 0;
        parsed_address = strtoull(address_text, &end_pointer, 16);
        if (end_pointer == address_text || errno == ERANGE ||
            parsed_address > UINT32_MAX) {
            parser_set_error(error_message, error_message_size,
                             "Invalid 32-bit hexadecimal address on trace line %zu.",
                             line_number);
            (void)fclose(trace);
            return false;
        }

        end_pointer = (char *)parser_skip_whitespace(end_pointer);
        if (*end_pointer != '\0') {
            parser_set_error(error_message, error_message_size,
                             "Unexpected text on trace line %zu.", line_number);
            (void)fclose(trace);
            return false;
        }

        if (!callback((uint32_t)parsed_address, line_number, context)) {
            parser_set_error(error_message, error_message_size,
                             "Trace processing stopped at line %zu.", line_number);
            (void)fclose(trace);
            return false;
        }
    }

    if (ferror(trace) != 0) {
        parser_set_error(error_message, error_message_size,
                         "I/O error while reading trace file.");
        (void)fclose(trace);
        return false;
    }

    if (fclose(trace) != 0) {
        parser_set_error(error_message, error_message_size,
                         "Could not close trace file '%s'.", trace_path);
        return false;
    }

    parser_set_error(error_message, error_message_size, "");
    return true;
}
