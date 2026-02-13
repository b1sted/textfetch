/* SPDX-License-Identifier: MIT */

#ifndef TERMINAL_OS_H
#define TERMINAL_OS_H

#include <stddef.h>

void term_get_shell(char *out_buf, const size_t buf_size);
void term_fallback_shell(char *out_buf, const size_t buf_size);
void term_sanitize_name(char *out_buf);

#endif /* TERMINAL_OS_H */