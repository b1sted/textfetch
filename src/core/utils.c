/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "config.h"
#include "ui.h"
#include "utils.h"

bool util_read_line(const char *path, char *out_buf, const size_t buf_size) {
    if (!out_buf || buf_size == 0) return false;
    out_buf[0] = '\0';

    int fd = open(path, O_RDONLY);
    if (fd != -1) {
        ssize_t bytes_read = read(fd, out_buf, buf_size - 1);
        if (bytes_read > 0) {          
            out_buf[strcspn(out_buf, "\n")] = '\0';
        } else {
            V_PRINTF("[ERROR] Fail to read %s\n", path);
            return false;
        }

        close(fd);
        return true;
    } else {
        V_PRINTF("[ERROR] Fail to open %s file\n", path);
        return false;
    }
}

bool util_read_uint8(const char *path, uint8_t *value) {
    char buf[TINY_BUFFER] = {0};
    util_read_line(path, buf, sizeof(buf));

    if (buf[0] == '\0') return false;

    *value = (uint8_t)strtoul(buf, NULL, 10);
    return true;
}

bool util_read_uint32(const char *path, uint32_t *value) {
    char buf[TINY_BUFFER] = {0};
    util_read_line(path, buf, sizeof(buf));

    if (buf[0] == '\0') return false;

    *value = (uint32_t)strtoul(buf, NULL, 10);
    return true;
}

bool util_read_hex(const char *path, uint32_t *value) {
    char buf[TINY_BUFFER] = {0};
    if (!util_read_line(path, buf, sizeof(buf)) || buf[0] == '\0') {
        return false;
    }

    char *endptr;
    errno = 0;

    unsigned long long val = strtoull(buf, &endptr, 16);

    if (endptr == buf) return false;

    if (errno == ERANGE) return false;

    if (val > 0xFFFFFFFFULL) return false;

    *value = (uint32_t)val;
    return true;
}

bool util_read_hex16(const char *path, uint16_t *value) {
    uint32_t val32 = 0;

    if (!util_read_hex(path, &val32)) return false;

    if (val32 > 0xFFFF) return false;

    *value = (uint16_t)val32;

    return true;
}

bool util_is_file_exist(const char *path) {
    return access(path, R_OK) == 0;
}

void util_format_size(double total_size, double used_size, char *out_buf, 
                      const size_t buf_size, data_unit_t from_unit) {
    static const char *memory_units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};

    static const double divisors[] = {
        1.0,               /* B */
        1024.0,            /* KiB */
        1048576.0,         /* MiB */
        1073741824.0,      /* GiB */
        1099511627776.0,   /* TiB */
        1125899906842624.0 /* PiB */
    };

    const uint8_t num_units = sizeof(divisors) / sizeof(divisors[0]);

    if ((uint8_t)from_unit < num_units) {
        total_size *= divisors[from_unit];
        used_size *= divisors[from_unit];
    }

    int8_t forced_unit = -1;
    uint8_t precision = 2;

    if (cfg_is_kib()) { forced_unit = 1; precision = 0; }
    if (cfg_is_mib()) { forced_unit = 2; precision = 0; }
    if (cfg_is_gib()) { forced_unit = 3; precision = 2; }

    uint8_t total_unit = 0;
    uint8_t used_unit = 0;

    const uint8_t usage_pct = (total_size > 1e-9) ? (uint8_t)((used_size / total_size) * 100) : 0;

    if (forced_unit != -1) {
        total_unit = forced_unit;
        used_unit = forced_unit;

        total_size /= divisors[forced_unit];
        used_size /= divisors[forced_unit];
    } else {
        uint8_t array_border = (sizeof(memory_units) / sizeof(memory_units[0]));

        while (total_size >= 1024 && total_unit < array_border - 1) {
            total_size /= 1024;
            total_unit++;
        }

        while (used_size >= 1024 && used_unit < array_border - 1) {
            used_size /= 1024;
            used_unit++;
        }
    }

    snprintf(out_buf, buf_size, "%.*f %s / %.*f %s (%hhu%%)", 
             precision, used_size, memory_units[used_unit], 
             precision, total_size, memory_units[total_unit], usage_pct);
}
