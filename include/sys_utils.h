/* SPDX-License-Identifier: MIT */

#ifndef SYS_UTILS_H
#define SYS_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "binary_trees.h"
#include "config.h"

/**
 * Verbose formatted print wrapper.
 * Sends output to stderr only if the verbose flag is set.
 */
#define V_PRINTF(fmt, ...)                       \
    do {                                         \
        if (cfg_is_verbose())                    \
            fprintf(stderr, fmt, ##__VA_ARGS__); \
    } while (0)

/* Standard macro to evaluate and return the smaller of two values. */
#define min(a, b) (((a) < (b)) ? (a) : (b))

/* Enumeration of standard byte capacity units for formatting sizes. */
typedef enum {
    UNIT_B = 0,
    UNIT_KIB,
    UNIT_MIB,
    UNIT_GIB,
    UNIT_TIB,
    UNIT_PIB
} data_unit_t;

/**
 * Reads the first line of a file into a buffer.
 *
 * @param path The absolute path to the file.
 * @param out_buf The buffer where the line will be stored.
 * @param buf_size The maximum size of the buffer.
 * @return true if successful, false otherwise.
 */
bool util_read_line(const char *path, char *out_buf, const size_t buf_size);

/**
 * Reads an 8-bit unsigned integer from a file.
 *
 * @param path The absolute path to the file.
 * @param value Pointer to store the result.
 * @return true if successful, false otherwise.
 */
bool util_read_uint8(const char *path, uint8_t *value);

/**
 * Reads a 16-bit unsigned integer from a file.
 *
 * @param path The absolute path to the file.
 * @param value Pointer to store the result.
 * @return true if successful, false otherwise.
 */
bool util_read_uint16(const char *path, uint16_t *value);

/**
 * Reads a 32-bit unsigned integer from a file.
 *
 * @param path The absolute path to the file.
 * @param value Pointer to store the result.
 * @return true if successful, false otherwise.
 */
bool util_read_uint32(const char *path, uint32_t *value);

/**
 * Reads a 16-bit signed integer from a file.
 *
 * @param path The absolute path to the file.
 * @param value Pointer to store the result.
 * @return true if successful, false otherwise.
 */
bool util_read_int16(const char *path, int16_t *value);

/**
 * Reads a 32-bit signed integer from a file.
 *
 * @param path The absolute path to the file.
 * @param value Pointer to store the result.
 * @return true if successful, false otherwise.
 */
bool util_read_int32(const char *path, int32_t *value);

/**
 * Reads a 16-bit unsigned integer in hexadecimal format from a file.
 *
 * @param path The absolute path to the file.
 * @param value Pointer to store the result.
 * @return true if successful, false otherwise.
 */
bool util_read_hex16(const char *path, uint16_t *value);

/**
 * Reads a 32-bit unsigned integer in hexadecimal format from a file.
 *
 * @param path The absolute path to the file.
 * @param value Pointer to store the result.
 * @return true if successful, false otherwise.
 */
bool util_read_hex(const char *path, uint32_t *value);

/**
 * Checks if a file exists and is readable.
 *
 * @param path The absolute path to the file.
 * @return true if the file exists and can be read, false otherwise.
 */
bool util_is_file_exist(const char *path);

/**
 * Checks if a string consists entirely of numeric digits.
 *
 * @param str The string to evaluate.
 * @return true if all characters are digits, false otherwise or if string is
 * empty.
 */
bool util_is_numeric_string(const char *str);

/**
 * Groups and formats an array of hardware strings, automatically adding
 * count prefixes (e.g. "2 x ") for duplicate entries.
 *
 * @param strings Array of strings representing hardware models.
 * @param count Number of elements in the array.
 * @param out_buf Buffer to store the formatted multi-line output.
 * @param buf_size Maximum size of the output buffer.
 */
void util_format_duplicate_hardware(const char **strings, uint8_t count,
                                    char *out_buf, size_t buf_size);

/**
 * Formats a byte size into a human-readable string (e.g., "MiB", "GiB").
 *
 * @param total_size The total capacity.
 * @param used_size The used capacity.
 * @param out_buf Buffer to store the formatted string.
 * @param buf_size Maximum size of the output buffer.
 * @param from_unit The unit of the input sizes (e.g., UNIT_KIB).
 */
void util_format_size(double total_size, double used_size, char *out_buf,
                      const size_t buf_size, data_unit_t from_unit);

/**
 * Parses a hardware ID database file into a binary tree forest.
 *
 * @param fp Pointer to the open IDs file (e.g., pci.ids).
 * @param capacity The expected number of top-level nodes (vendors).
 * @return A tree containing the parsed hardware mappings, or NULL on failure.
 */
forest *util_parse_ids_file(FILE *fp, size_t capacity);

#endif /* SYS_UTILS_H */