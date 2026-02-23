/* SPDX-License-Identifier: MIT */

#ifndef TERMINAL_OS_H
#define TERMINAL_OS_H

#include <stddef.h>

/* Discovers and formats the name of the user's active shell. */
void term_get_shell(char *out_buf, const size_t buf_size);

/* Fallback method to identify shell via env vars or login configs. */
void term_fallback_shell(char *out_buf, const size_t buf_size);

/* Sanitizes shell executable name, stripping prefixes and paths. */
void term_sanitize_name(char *out_buf);

#endif /* TERMINAL_OS_H */