/* SPDX-License-Identifier: MIT */

#ifndef SYS_UTILS_H
#define SYS_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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

#define min(a, b) (((a) < (b)) ? (a) : (b))

typedef enum {
    UNIT_B = 0,
    UNIT_KIB,
    UNIT_MIB,
    UNIT_GIB,
    UNIT_TIB,
    UNIT_PIB
} data_unit_t;

bool util_read_line(const char *path, char *out_buf, const size_t buf_size);

bool util_read_uint8(const char *path, uint8_t *value);
bool util_read_uint16(const char *path, uint16_t *value);
bool util_read_uint32(const char *path, uint32_t *value);

bool util_read_int16(const char *path, int16_t *value);

bool util_read_hex16(const char *path, uint16_t *value);
bool util_read_hex(const char *path, uint32_t *value);

bool util_is_file_exist(const char *path);

void util_format_size(double total_size, double used_size, char *out_buf, 
                      const size_t buf_size, data_unit_t from_unit);

#endif /* SYS_UTILS_H */