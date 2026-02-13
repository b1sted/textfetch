/* SPDX-License-Identifier: MIT */

#ifndef CAPTURE_H
#define CAPTURE_H

#include <stddef.h>

int capture_line(const char *command, const char *arg, char *out_buf, const size_t buf_size);

#endif /* CAPTURE_H */