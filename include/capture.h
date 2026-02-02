/* SPDX-License-Identifier: MIT */

#ifndef CAPTURE_H
#define CAPTURE_H

#include <stddef.h>

/**
 * Executes a shell command and captures its standard output into a buffer.
 * 
 * This function reads only the first line of the command's output. 
 * The trailing newline character ('\n'), if present, is automatically removed.
 * The resulting string in out_buf is always null-terminated.
 *
 * @param command  The shell command to execute.
 * @param out_buf  Buffer to store the captured line.
 * @param buf_size Size of the output buffer.
 * @return 0 on success, non-zero on error (e.g., process execution failed).
 */
int capture_line(const char *command, char *out_buf, size_t buf_size);

#endif // CAPTURE_H