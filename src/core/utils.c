/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <fcntl.h>
#include <unistd.h>

#include "ui.h"
#include "utils.h"

void util_read_line(const char *path, char *out_buf, const size_t buf_size) {
    if (!out_buf || buf_size == 0) return;
    out_buf[0] = '\0';

    int fd = open(path, O_RDONLY);
    if (fd != -1) {
        ssize_t bytes_read = read(fd, out_buf, buf_size - 1);
        if (bytes_read > 0) {          
            out_buf[strcspn(out_buf, "\n")] = '\0';
        } else {
            V_PRINTF("[ERROR] Fail to read %s\n", path);
        }

        close(fd);
    } else {
        V_PRINTF("[ERROR] Fail to open %s file\n", path);
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
