/* SPDX-License-Identifier: MIT */

#ifndef CAPTURE_H
#define CAPTURE_H

#include <stddef.h>

/**
 * Executes a command and captures its first line of output.
 *
 * @param command The command to execute.
 * @param arg An optional argument to pass to the command (can be NULL).
 * @param out_buf Buffer to store the captured output.
 * @param buf_size The maximum size of the output buffer.
 * @return 0 on success, 1 if the command fails or produces no output,
 *         or -1 if the capture pipeline cannot be initialized.
 */
int capture_line(const char *command, const char *arg,
                 char *out_buf, const size_t buf_size);

#endif /* CAPTURE_H */