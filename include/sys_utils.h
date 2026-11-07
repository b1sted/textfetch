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
#define V_PRINTF(...)                                                                              \
    do {                                                                                           \
        if (cfg_get_verbose()) {                                                                   \
            fprintf(stderr, __VA_ARGS__);                                                          \
        }                                                                                          \
    } while (0)

/* Enumeration of standard byte capacity units for formatting sizes. */
typedef enum {
    UNIT_B = 0,
    UNIT_KIB,
    UNIT_MIB,
    UNIT_GIB,
    UNIT_TIB,
    UNIT_PIB,
    UNIT_AUTO
} data_unit_t;

/**
 * Returns the unit currently forced for size formatting.
 *
 * @return The active output unit, or UNIT_AUTO when sizes are scaled
 * automatically.
 */
data_unit_t util_get_output_unit(void);

/**
 * Sets the unit used for size formatting. The most recent call wins, which
 * makes command-line flag order determine the final output format.
 *
 * @param unit The unit to force, or UNIT_AUTO for automatic scaling.
 */
void util_set_output_unit(data_unit_t unit);

/**
 * Searches for any string from a null-terminated array within a target string.
 *
 * @param str The target string to search within.
 * @param array A NULL-terminated array of strings to search for.
 * @return A pointer to the first occurrence of a matched substring, or NULL if not found.
 */
const char *util_string_in_array(const char *str, const char *const array[]);

/**
 * Looks up a string key in a 2D dictionary array and returns its mapped value.
 *
 * @param str The key string to look up.
 * @param dict A 2D array of string pairs {key, value}.
 * @param dict_size The number of pairs in the dictionary.
 * @return The mapped value string, or NULL if the key is not found.
 */
const char *util_string_lookup(const char *str, const char *const dict[][2],
                               const size_t dict_size);

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
void util_format_duplicate_hardware(const char *const strings[], uint8_t count, char *out_buf,
                                    size_t buf_size);

/**
 * Formats a byte size into a human-readable string (e.g., "MiB", "GiB").
 *
 * @param total_size The total capacity.
 * @param used_size The used capacity.
 * @param out_buf Buffer to store the formatted string.
 * @param buf_size Maximum size of the output buffer.
 * @param from_unit The unit of the input sizes (e.g., UNIT_KIB).
 */
void util_format_size(double total_size, double used_size, char *out_buf, const size_t buf_size,
                      data_unit_t from_unit);

/**
 * A single vendor/device lookup request for a filtered IDs parse.
 */
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
} util_ids_query_t;

/**
 * Parses a hardware ID database file into a binary tree forest.
 *
 * The file is memory-mapped and parsed in place, so no per-line copies are
 * made and arbitrarily long lines are supported. Names are copied into the
 * returned nodes before the mapping is released.
 *
 * @param path Path to the IDs database file (e.g., pci.ids).
 * @param capacity The expected number of top-level nodes (vendors).
 * @return A tree containing the parsed hardware mappings, or NULL on failure.
 */
tree_forest_t *util_parse_ids_file(const char *path, size_t capacity);

/**
 * Parses only the requested vendor/device pairs from a memory-mapped IDs file.
 *
 * Parsing stops as soon as every query has been resolved, so callers that need
 * a few GPUs do not pay for building the complete pci.ids tree. The returned
 * forest contains the matching vendors and devices; unrelated devices are not
 * stored.
 *
 * @param path Path to the IDs database file (e.g., pci.ids).
 * @param capacity The expected number of top-level nodes (vendors).
 * @param queries Array of vendor/device pairs to resolve.
 * @param query_count Number of entries in @p queries; must be greater than 0.
 * @return A tree containing the matching mappings, or NULL on failure.
 */
tree_forest_t *util_parse_ids_file_filtered(const char *path, size_t capacity,
                                            const util_ids_query_t *queries, size_t query_count);

/**
 * Parses a hardware ID database from an in-memory buffer.
 *
 * This is used for embedded databases (e.g., arm.ids) that are not backed by a
 * file. The buffer is read-only and does not need to be NUL-terminated.
 *
 * @param data Pointer to the database contents.
 * @param size Number of bytes in @p data.
 * @param capacity The expected number of top-level nodes (vendors).
 * @return A tree containing the parsed hardware mappings, or NULL on failure.
 */
tree_forest_t *util_parse_ids_buffer(const char *data, size_t size, size_t capacity);

/**
 * Parses only the requested vendor/device pairs from an in-memory IDs buffer.
 *
 * Behaves like util_parse_ids_file_filtered() and stops once all queries are
 * resolved.
 *
 * @param data Pointer to the database contents.
 * @param size Number of bytes in @p data.
 * @param capacity The expected number of top-level nodes (vendors).
 * @param queries Array of vendor/device pairs to resolve.
 * @param query_count Number of entries in @p queries; must be greater than 0.
 * @return A tree containing the matching mappings, or NULL on failure.
 */
tree_forest_t *util_parse_ids_buffer_filtered(const char *data, size_t size, size_t capacity,
                                              const util_ids_query_t *queries, size_t query_count);

#endif /* SYS_UTILS_H */