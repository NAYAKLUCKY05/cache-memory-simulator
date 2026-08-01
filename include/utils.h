#ifndef CACHE_SIMULATOR_UTILS_H
#define CACHE_SIMULATOR_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Tests whether a positive integer is a power of two.
 *
 * @param value Value to test.
 * @return true only when value is a power of two.
 */
bool utils_is_power_of_two(size_t value);

/**
 * Returns log2(value) for a positive power-of-two value.
 *
 * @param value Positive power-of-two value.
 * @return Base-two logarithm of value, or 0 for an invalid input.
 */
uint8_t utils_log2_power_of_two(size_t value);

/**
 * Copies a diagnostic message into a caller-owned buffer when one is supplied.
 *
 * @param destination Destination buffer, which may be NULL.
 * @param destination_size Size of destination in bytes.
 * @param message Null-terminated diagnostic message.
 */
void utils_set_error(char *destination, size_t destination_size,
                     const char *message);

#endif
