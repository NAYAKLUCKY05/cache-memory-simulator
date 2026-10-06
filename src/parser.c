#include "parser.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 256U

static void set_error(char *buffer, size_t size, const char *format, ...)
{
    va_list args;

    if (buffer == NULL || size == 0U) {
        return;
    }
    va_start(args, format);
    (void)vsnprintf(buffer, size, format, args);
    va_end(args);
}

static const char *skip_spaces(const char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }
    return text;
}

static bool skip_rest_of_line(FILE *file)
{
    int c;

    do {
        c = fgetc(file);
    } while (c != '\n' && c != EOF);
    return ferror(file) == 0;
}

bool parser_parse_line(const char *line, AccessType *type, uint32_t *address,
                       bool *is_entry)
{
    const char *p;
    char *end;
    unsigned long long value;

    *is_entry = false;
    p = skip_spaces(line);
    if (*p == '\0' || *p == '#') {
        return false;
    }
    *is_entry = true;

    *type = ACCESS_READ;
    if ((p[0] == 'R' || p[0] == 'r' || p[0] == 'W' || p[0] == 'w') &&
        isspace((unsigned char)p[1])) {
        *type = (p[0] == 'W' || p[0] == 'w') ? ACCESS_WRITE : ACCESS_READ;
        p = skip_spaces(p + 1);
    }

    if (*p == '+' || *p == '-') {
        return false;
    }

    errno = 0;
    value = strtoull(p, &end, 16);
    if (end == p || errno == ERANGE || value > UINT32_MAX) {
        return false;
    }

    if (*skip_spaces(end) != '\0') {
        return false;
    }

    *address = (uint32_t)value;
    return true;
}

bool parser_read_trace(const char *trace_path, TraceCallback callback,
                       void *context, char *error_message,
                       size_t error_message_size)
{
    char line[MAX_LINE_LENGTH];
    FILE *file;
    size_t line_number = 0U;
    bool ok = true;

    if (trace_path == NULL || trace_path[0] == '\0' || callback == NULL) {
        set_error(error_message, error_message_size,
                  "A trace path and callback are required.");
        return false;
    }

    file = fopen(trace_path, "r");
    if (file == NULL) {
        set_error(error_message, error_message_size,
                  "Could not open trace file '%s'.", trace_path);
        return false;
    }

    while (ok && fgets(line, sizeof(line), file) != NULL) {
        AccessType type;
        uint32_t address;
        bool is_entry;
        bool complete = strchr(line, '\n') != NULL || feof(file);

        ++line_number;

        if (!complete) {
            (void)skip_rest_of_line(file);
            set_error(error_message, error_message_size,
                      "Line %zu is longer than %u characters.", line_number,
                      MAX_LINE_LENGTH - 1U);
            ok = false;
        } else if (!parser_parse_line(line, &type, &address, &is_entry)) {
            if (is_entry) {
                set_error(error_message, error_message_size,
                          "Invalid trace entry on line %zu.", line_number);
                ok = false;
            }
        } else if (!callback(type, address, line_number, context)) {
            set_error(error_message, error_message_size,
                      "Trace processing stopped at line %zu.", line_number);
            ok = false;
        }
    }

    if (ok && ferror(file) != 0) {
        set_error(error_message, error_message_size,
                  "I/O error while reading the trace file.");
        ok = false;
    }

    (void)fclose(file);
    if (ok) {
        set_error(error_message, error_message_size, "");
    }
    return ok;
}
