/* SPDX-License-Identifier: MIT */

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stdint.h>

void util_read_line(const char *path, char *out_buf, const size_t buf_size);
bool util_read_uint8(const char *path, uint8_t *value);
bool util_read_uint32(const char *path, uint32_t *value);

#endif /* UTILS_H */