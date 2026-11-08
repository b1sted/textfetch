/* SPDX-License-Identifier: MIT */

#ifndef TERMINAL_OS_H
#define TERMINAL_OS_H

#include <stddef.h>

/* Discovers and formats the name of the user's active shell.
 *
 * Walks up the parent process chain to skip launchers and wrappers such as
 * linker64, sudo, or "sh -c" and reaches the shell that owns the terminal. */
void term_get_shell(char *out_buf, const size_t buf_size);

/* Starts shell discovery and version capture without waiting for completion.
 *
 * Call this before slow system or hardware probes so the shell process can
 * run in parallel. term_get_shell() collects the result and remains usable
 * when this function is not called. */
void term_prepare_shell(void);

/* Fallback method to identify shell via env vars or login configs. */
void term_fallback_shell(char *out_buf, const size_t buf_size);

/* Sanitizes shell executable name, stripping prefixes and paths. */
void term_sanitize_name(char *out_buf);

#endif /* TERMINAL_OS_H */