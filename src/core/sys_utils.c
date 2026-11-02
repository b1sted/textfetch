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
        1.0,               /* B */                                                                 \
        1024.0,            /* KiB */                                                               \
        1048576.0,         /* MiB */                                                               \
        1073741824.0,      /* GiB */                                                               \
        1099511627776.0,   /* TiB */                                                               \
        1125899906842624.0 /* PiB */                                                               \
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

tree_forest_t *util_parse_ids_file(FILE *fp, size_t capacity)
{
    tree_forest_t *ids_forest = NULL;
    tree_forest_t *result = NULL;
    char line[LINE_BUFFER] = {0};
    tree_node_t *branch = NULL;

    if (!fp) {
        goto cleanup;
    }

    if (capacity == 0) {
        goto cleanup;
    }

    ids_forest = tree_create_forest(capacity);
    if (!ids_forest) {
        goto cleanup;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n' || line[1] == '\t') {
            continue;
        }

        if (isalnum((unsigned char)line[0])) {
            char *vendor_name = NULL;
            uint16_t vendor_id = (uint16_t)strtoul(line, &vendor_name, 16);

            line[strcspn(line, "\r\n")] = '\0';

            while (isspace((unsigned char)*vendor_name)) {
                vendor_name++;
            }

            branch = tree_create_node(vendor_id, vendor_name);
            tree_add_to_forest(ids_forest, branch);
        } else if (line[0] == '\t') {
            char *device_name = NULL;
            uint16_t device_id = (uint16_t)strtoul(line, &device_name, 16);

            line[strcspn(line, "\r\n")] = '\0';

            while (isspace((unsigned char)*device_name)) {
                device_name++;
            }

            device_name[strcspn(device_name, "\n")] = '\0';
            tree_add_child(branch, device_id, device_name);
        }
    }

    result = ids_forest;
    ids_forest = NULL;

cleanup:
    if (fp) {
        fclose(fp);
    }
    if (ids_forest) {
        tree_destroy_forest(ids_forest);
    }
    return result;
}
