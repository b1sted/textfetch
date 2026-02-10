/* SPDX-License-Identifier: MIT */

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stdint.h>

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
bool util_read_uint32(const char *path, uint32_t *value);

bool util_read_hex16(const char *path, uint16_t *value);
bool util_read_hex(const char *path, uint32_t *value);

bool util_is_file_exist(const char *path);

void util_format_size(double total_size, double used_size, char *out_buf, 
                      const size_t buf_size, data_unit_t from_unit);

#endif /* UTILS_H */