/* SPDX-License-Identifier: MIT */

#include "sys_utils.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "binary_trees.h"
#include "config.h"
#include "defs.h"

/* Human-readable unit suffixes for formatting byte sizes. */
#define MEMORY_UNITS                                                                               \
    {                                                                                              \
        "B", "KiB", "MiB", "GiB", "TiB", "PiB",                                                    \
    }

/* Divisors used to convert raw bytes into larger capacities. */
#define MEMORY_DIVISORS                                                                            \
    {                                                                                              \
        1.0,                   /* B */                                                             \
            1024.0,            /* KiB */                                                           \
            1048576.0,         /* MiB */                                                           \
            1073741824.0,      /* GiB */                                                           \
            1099511627776.0,   /* TiB */                                                           \
            1125899906842624.0 /* PiB */                                                           \
    }

/**
 * Reads a single line from a file and parses it as a base-10 long integer.
 * Acts as a generic backbone for typed unsigned/signed integer readers.
 *
 * @param path The absolute path to the file to read.
 * @param out_val Pointer to the long variable to store the parsed value.
 * @return true if the file was read and a value was parsed, false otherwise.
 */
static bool util_parse_int64(const char *path, int64_t *out_val);

/* Output unit forced by the command line; UNIT_AUTO enables scaling. */
static data_unit_t output_unit = UNIT_AUTO;

data_unit_t util_get_output_unit(void) { return output_unit; }

void util_set_output_unit(data_unit_t unit) { output_unit = unit; }

const char *util_string_in_array(const char *str, const char *const array[])
{
    if (!str || !array) {
        return NULL;
    }

    for (size_t i = 0; array[i] != NULL; i++) {
        const char *ptr = strstr(str, array[i]);
        if (ptr) {
            return ptr;
        }
    }

    return NULL;
}

const char *util_string_lookup(const char *str, const char *const dict[][2], const size_t dict_size)
{
    if (!str || !dict || dict_size == 0) {
        return NULL;
    }

    for (size_t i = 0; i < dict_size; i++) {
        if (strcmp(str, dict[i][0]) == 0) {
            return dict[i][1];
        }
    }

    return NULL;
}

bool util_read_line(const char *path, char *out_buf, const size_t buf_size)
{
    int fd = -1;
    bool ok = false;

    if (!path || !out_buf || buf_size == 0) {
        return false;
    }

    out_buf[0] = '\0';

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        V_PRINTF("[ERROR] Failed to open %s file\n", path);
        goto cleanup;
    }

    ssize_t bytes_read = read(fd, out_buf, buf_size - 1);
    if (bytes_read > 0) {
        out_buf[bytes_read] = '\0';
        out_buf[strcspn(out_buf, "\n")] = '\0';
        ok = true;
    } else {
        V_PRINTF("[ERROR] Failed to read %s\n", path);
    }

cleanup:
    if (fd != -1) {
        close(fd);
    }
    return ok;
}

bool util_read_uint8(const char *path, uint8_t *value)
{
    if (!value) {
        return false;
    }

    int64_t raw_value = 0;
    if (!util_parse_int64(path, &raw_value)) {
        return false;
    }

    if (raw_value < 0 || raw_value > UINT8_MAX) {
        return false;
    }

    *value = (uint8_t)raw_value;
    return true;
}

bool util_read_uint16(const char *path, uint16_t *value)
{
    if (!value) {
        return false;
    }

    int64_t raw_value = 0;
    if (!util_parse_int64(path, &raw_value)) {
        return false;
    }

    if (raw_value < 0 || raw_value > UINT16_MAX) {
        return false;
    }

    *value = (uint16_t)raw_value;
    return true;
}

bool util_read_uint32(const char *path, uint32_t *value)
{
    if (!value) {
        return false;
    }

    int64_t raw_value = 0;
    if (!util_parse_int64(path, &raw_value)) {
        return false;
    }

    if (raw_value < 0 || raw_value > UINT32_MAX) {
        return false;
    }

    *value = (uint32_t)raw_value;
    return true;
}

bool util_read_int16(const char *path, int16_t *value)
{
    if (!value) {
        return false;
    }

    int64_t raw_value = 0;
    if (!util_parse_int64(path, &raw_value)) {
        return false;
    }

    if (raw_value < INT16_MIN || raw_value > INT16_MAX) {
        return false;
    }

    *value = (int16_t)raw_value;
    return true;
}

bool util_read_int32(const char *path, int32_t *value)
{
    if (!value) {
        return false;
    }

    int64_t raw_value = 0;
    if (!util_parse_int64(path, &raw_value)) {
        return false;
    }

    if (raw_value < INT32_MIN || raw_value > INT32_MAX) {
        return false;
    }

    *value = (int32_t)raw_value;
    return true;
}

static bool util_parse_int64(const char *path, int64_t *out_val)
{
    char buf[TINY_BUFFER] = {0};

    if (!out_val || !util_read_line(path, buf, sizeof(buf))) {
        return false;
    }

    char *endptr = NULL;
    errno = 0;

    intmax_t value = strtoimax(buf, &endptr, 10);
    if (endptr == buf || *endptr != '\0' || errno == ERANGE) {
        return false;
    }

    int64_t narrowed = (int64_t)value;
    if ((intmax_t)narrowed != value) {
        return false;
    }

    *out_val = narrowed;
    return true;
}

bool util_read_hex(const char *path, uint32_t *value)
{
    if (!value) {
        return false;
    }

    char buf[TINY_BUFFER] = {0};
    if (!util_read_line(path, buf, sizeof(buf)) || buf[0] == '\0') {
        return false;
    }

    char *endptr = NULL;
    errno = 0;

    uintmax_t val = strtoumax(buf, &endptr, 16);

    if (endptr == buf || *endptr != '\0') {
        return false;
    }

    if (errno == ERANGE) {
        return false;
    }

    if (val > UINT32_MAX) {
        return false;
    }

    *value = (uint32_t)val;
    return true;
}

bool util_read_hex16(const char *path, uint16_t *value)
{
    if (!value) {
        return false;
    }

    uint32_t val32 = 0;

    if (!util_read_hex(path, &val32)) {
        return false;
    }

    if (val32 > UINT16_MAX) {
        return false;
    }

    *value = (uint16_t)val32;

    return true;
}

bool util_is_file_exist(const char *path) { return path != NULL && access(path, R_OK) == 0; }

bool util_is_numeric_string(const char *str)
{
    if (!str || *str == '\0') {
        return false;
    }

    const char *cursor = str;
    while (*cursor) {
        if (!isdigit((unsigned char)*cursor)) {
            return false;
        }
        cursor++;
    }

    return true;
}

void util_format_duplicate_hardware(const char *const strings[], uint8_t count, char *out_buf,
                                    size_t buf_size)
{
    if (!strings || !out_buf || buf_size == 0) {
        return;
    }

    for (uint8_t i = 0; i < count; i++) {
        if (!strings[i] || strings[i][0] == '\0') {
            continue;
        }

        bool already_printed = false;
        for (uint8_t j = 0; j < i; j++) {
            if (strings[j] && strcmp(strings[i], strings[j]) == 0) {
                already_printed = true;
                break;
            }
        }

        if (already_printed) {
            continue;
        }

        uint8_t dup_count = 1;
        for (uint8_t k = i + 1; k < count; k++) {
            if (strings[k] && strcmp(strings[i], strings[k]) == 0) {
                dup_count++;
            }
        }

        char prefix[HEX_BUFFER] = "";
        if (dup_count > 1) {
            snprintf(prefix, sizeof(prefix), "%" PRIu8 " x ", dup_count);
        }

        size_t offset = strlen(out_buf);
        if (offset >= buf_size) {
            break;
        }

        snprintf(out_buf + offset, buf_size - offset, "%s%s%s",
                 (out_buf[0] == '\0') ? "" : "\n     ", prefix, strings[i]);
    }
}

void util_format_size(double total_size, double used_size, char *out_buf, const size_t buf_size,
                      data_unit_t from_unit)
{
    if (!out_buf || buf_size == 0) {
        return;
    }

    static const char *memory_units[] = MEMORY_UNITS;
    static const double divisors[] = MEMORY_DIVISORS;

    const size_t num_units = sizeof(divisors) / sizeof(divisors[0]);

    if ((size_t)from_unit < num_units) {
        total_size *= divisors[from_unit];
        used_size *= divisors[from_unit];
    }

    data_unit_t forced_unit = util_get_output_unit();
    uint8_t precision = 2;

    if (forced_unit == UNIT_KIB || forced_unit == UNIT_MIB) {
        precision = 0;
    }

    size_t unit = UNIT_B;

    if (forced_unit != UNIT_AUTO && (size_t)forced_unit < num_units) {
        unit = (size_t)forced_unit;
    } else {
        double scale = (total_size > used_size) ? total_size : used_size;

        while (scale >= 1024.0 && unit < num_units - 1) {
            scale /= 1024.0;
            unit++;
        }
    }

    total_size /= divisors[unit];
    used_size /= divisors[unit];

    double usage_pct = 0.0;
    if (total_size > 1e-9) {
        usage_pct = (used_size / total_size) * 100.0;
        if (usage_pct < 0.0) {
            usage_pct = 0.0;
        }
        if (usage_pct > 100.0) {
            usage_pct = 100.0;
        }
    }

    snprintf(out_buf, buf_size, "%.*f %s / %.*f %s (%hhu%%)", precision, used_size,
             memory_units[unit], precision, total_size, memory_units[unit], (uint8_t)usage_pct);
}

/**
 * Returns the hexadecimal value of a single character.
 *
 * @param c Character to convert.
 * @return A value in the range [0, 15], or -1 when @p c is not a hex digit.
 */
static int util_hex_digit_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/**
 * Parses a hexadecimal identifier from a bounded line span.
 *
 * The parser stops after four hex digits (the widest identifier used by
 * pci.ids and arm.ids) and rejects tokens that continue with another hex digit.
 *
 * @param cursor Pointer to the current position; advanced past the token.
 * @param end Pointer one byte past the line.
 * @param out_id Receives the parsed identifier.
 * @return true when a valid identifier was parsed, false otherwise.
 */
static bool util_parse_ids_hex(const char **cursor, const char *end, uint16_t *out_id)
{
    const char *p = NULL;
    uint32_t value = 0;
    size_t digits = 0;

    if (!cursor || !*cursor || !end || !out_id) {
        return false;
    }

    p = *cursor;
    while (p < end && digits < 4) {
        int digit = util_hex_digit_value(*p);
        if (digit < 0) {
            break;
        }
        value = (value << 4) | (uint32_t)digit;
        p++;
        digits++;
    }

    if (digits == 0 || (p < end && util_hex_digit_value(*p) >= 0)) {
        return false;
    }

    *cursor = p;
    *out_id = (uint16_t)value;
    return true;
}

/**
 * Extracts a trimmed name span after an identifier token.
 *
 * Leading whitespace is skipped and trailing whitespace (including CR from
 * CRLF files) is removed. The returned span is not NUL-terminated.
 *
 * @param cursor Start of the remainder of the line.
 * @param end Pointer one byte past the line.
 * @param out_len Receives the number of bytes in the name.
 * @return Pointer to the first name byte.
 */
static const char *util_parse_ids_name(const char *cursor, const char *end, size_t *out_len)
{
    const char *name_end = NULL;

    if (!cursor || !end || !out_len) {
        if (out_len) {
            *out_len = 0;
        }
        return cursor;
    }

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }

    name_end = end;
    while (name_end > cursor && isspace((unsigned char)name_end[-1])) {
        name_end--;
    }

    *out_len = (size_t)(name_end - cursor);
    return cursor;
}

/**
 * Checks whether a vendor line is relevant to at least one unresolved query.
 *
 * @param queries Query array, or NULL for a full parse.
 * @param query_count Number of queries.
 * @param found Per-query resolution flags.
 * @param vendor_id Vendor identifier from the current line.
 * @return true when the line must be stored.
 */
static bool util_ids_vendor_is_wanted(const util_ids_query_t *queries, size_t query_count,
                                      const bool *found, uint16_t vendor_id)
{
    if (!queries) {
        return true;
    }

    if (!found) {
        return false;
    }

    for (size_t i = 0; i < query_count; i++) {
        if (!found[i] && queries[i].vendor_id == vendor_id) {
            return true;
        }
    }

    return false;
}

/**
 * Checks whether a device line resolves a pending query.
 *
 * All duplicate queries that match the line are marked as resolved.
 *
 * @param queries Query array, or NULL for a full parse.
 * @param query_count Number of queries.
 * @param found Per-query resolution flags.
 * @param vendor_id Vendor identifier of the current device.
 * @param device_id Device identifier from the current line.
 * @param remaining Number of unresolved queries; decremented on a match.
 * @return true when the line must be stored.
 */
static bool util_ids_device_is_wanted(const util_ids_query_t *queries, size_t query_count,
                                      bool *found, uint16_t vendor_id, uint16_t device_id,
                                      size_t *remaining)
{
    bool wanted = false;

    if (!queries) {
        return true;
    }

    if (!found || !remaining) {
        return false;
    }

    for (size_t i = 0; i < query_count; i++) {
        if (!found[i] && queries[i].vendor_id == vendor_id && queries[i].device_id == device_id) {
            found[i] = true;
            wanted = true;
            if (*remaining > 0) {
                (*remaining)--;
            }
        }
    }

    return wanted;
}

/**
 * Parses an IDs database span, optionally filtering for specific queries.
 *
 * @param data Database contents, or NULL when @p size is 0.
 * @param size Number of bytes in @p data.
 * @param capacity Expected number of root vendors.
 * @param queries Query array, or NULL for a full parse.
 * @param query_count Number of queries.
 * @return The parsed forest, or NULL on failure.
 */
static tree_forest_t *util_parse_ids_span(const char *data, size_t size, size_t capacity,
                                          const util_ids_query_t *queries, size_t query_count)
{
    tree_forest_t *ids_forest = NULL;
    tree_forest_t *result = NULL;
    tree_node_t *branch = NULL;
    bool *found = NULL;
    const char *cursor = NULL;
    const char *end = NULL;
    size_t remaining = query_count;

    if (capacity == 0 || (!data && size > 0) || (!queries && query_count > 0) ||
        (queries && query_count == 0)) {
        goto cleanup;
    }

    if (queries) {
        if (query_count > SIZE_MAX / sizeof(*found)) {
            goto cleanup;
        }
        found = calloc(query_count, sizeof(*found));
        if (!found) {
            goto cleanup;
        }
    }

    ids_forest = tree_create_forest(capacity);
    if (!ids_forest) {
        goto cleanup;
    }

    if (size > 0) {
        cursor = data;
        end = data + size;
    }

    while (cursor && cursor < end) {
        const char *line = cursor;
        const char *line_end = memchr(line, '\n', (size_t)(end - line));
        size_t indent = 0;

        if (line_end) {
            cursor = line_end + 1;
        } else {
            line_end = end;
            cursor = end;
        }

        if (line == line_end || line[0] == '#') {
            continue;
        }

        /* "C <class>" starts the device-class tail, not another vendor. */
        if (line[0] == 'C' && line + 1 < line_end && isspace((unsigned char)line[1])) {
            break;
        }

        while (line + indent < line_end && line[indent] == '\t') {
            indent++;
        }

        /* Subsystem and prog-if entries are two or more tabs deep. */
        if (indent > 1) {
            continue;
        }

        const char *token = line + indent;
        uint16_t id = 0;

        if (!util_parse_ids_hex(&token, line_end, &id)) {
            continue;
        }

        size_t name_len = 0;
        const char *name = util_parse_ids_name(token, line_end, &name_len);

        if (indent == 0) {
            if (util_ids_vendor_is_wanted(queries, query_count, found, id)) {
                branch = tree_create_node_n(id, name, name_len);
                if (branch) {
                    tree_add_to_forest(ids_forest, branch);
                }
            } else {
                branch = NULL;
            }
        } else if (branch && util_ids_device_is_wanted(queries, query_count, found, branch->id, id,
                                                       &remaining)) {
            tree_add_child_n(branch, id, name, name_len);
        }

        if (queries && remaining == 0) {
            break;
        }
    }

    result = ids_forest;
    ids_forest = NULL;

cleanup:
    tree_destroy_forest(ids_forest);
    free(found);
    return result;
}

tree_forest_t *util_parse_ids_buffer(const char *data, size_t size, size_t capacity)
{
    return util_parse_ids_span(data, size, capacity, NULL, 0);
}

tree_forest_t *util_parse_ids_buffer_filtered(const char *data, size_t size, size_t capacity,
                                              const util_ids_query_t *queries, size_t query_count)
{
    if (!queries || query_count == 0) {
        return NULL;
    }

    return util_parse_ids_span(data, size, capacity, queries, query_count);
}

/**
 * Memory-maps an IDs file and parses it with an optional query filter.
 *
 * @param path Path to the IDs database file.
 * @param capacity Expected number of root vendors.
 * @param queries Query array, or NULL for a full parse.
 * @param query_count Number of queries.
 * @return The parsed forest, or NULL on failure.
 */
static tree_forest_t *util_parse_ids_path(const char *path, size_t capacity,
                                          const util_ids_query_t *queries, size_t query_count)
{
    tree_forest_t *ids_forest = NULL;
    struct stat file_stat = {0};
    void *mapped = MAP_FAILED;
    size_t length = 0;
    int fd = -1;

    if (!path || capacity == 0) {
        goto cleanup;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        goto cleanup;
    }

    if (fstat(fd, &file_stat) != 0 || !S_ISREG(file_stat.st_mode) || file_stat.st_size < 0) {
        goto cleanup;
    }

    if ((uintmax_t)file_stat.st_size > (uintmax_t)SIZE_MAX) {
        goto cleanup;
    }

    length = (size_t)file_stat.st_size;

    if (length == 0) {
        ids_forest = util_parse_ids_span(NULL, 0, capacity, queries, query_count);
        goto cleanup;
    }

    mapped = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        goto cleanup;
    }

    ids_forest = util_parse_ids_span((const char *)mapped, length, capacity, queries, query_count);

cleanup:
    if (mapped != MAP_FAILED) {
        munmap(mapped, length);
    }
    if (fd >= 0) {
        close(fd);
    }
    return ids_forest;
}

tree_forest_t *util_parse_ids_file(const char *path, size_t capacity)
{
    return util_parse_ids_path(path, capacity, NULL, 0);
}

tree_forest_t *util_parse_ids_file_filtered(const char *path, size_t capacity,
                                            const util_ids_query_t *queries, size_t query_count)
{
    if (!queries || query_count == 0) {
        return NULL;
    }

    return util_parse_ids_path(path, capacity, queries, query_count);
}
