/* SPDX-License-Identifier: MIT */

#ifndef CAPTURE_H
#define CAPTURE_H

#include <stddef.h>
#include <sys/types.h>

/**
 * State of a command capture that runs in the background.
 *
 * Initialize it with capture_job_init() before the first use. A job is active
 * between a successful capture_start() and the matching capture_finish().
 */
typedef struct {
    int read_fd;
    pid_t pid;
} capture_job_t;

/**
 * Initializes a capture job to the idle state.
 *
 * @param job The job to initialize.
 */
void capture_job_init(capture_job_t *job);

/**
 * Starts a command without waiting for it to finish.
 *
 * The command's standard input and standard error are redirected to
 * /dev/null, while standard output is captured by the returned job.
 *
 * @param job The job that owns the spawned process.
 * @param command The command to execute.
 * @param arg An optional argument to pass to the command (can be NULL).
 * @return 0 on success, or -1 on invalid arguments or when the capture
 *         pipeline cannot be initialized.
 */
int capture_start(capture_job_t *job, const char *command, const char *arg);

/**
 * Waits for a started command and captures its first line of output.
 *
 * @param job A job previously started with capture_start().
 * @param out_buf Buffer to store the captured output.
 * @param buf_size The maximum size of the output buffer.
 * @return 0 on success, 1 if the command fails or produces no output,
 *         or -1 on invalid arguments.
 */
int capture_finish(capture_job_t *job, char *out_buf, const size_t buf_size);

/**
 * Executes a command and captures its first line of output.
 *
 * @param command The command to execute.
 * @param arg An optional argument to pass to the command (can be NULL).
 * @param out_buf Buffer to store the captured output.
 * @param buf_size The maximum size of the output buffer.
 * @return 0 on success, 1 if the command fails or produces no output,
 *         or -1 on invalid arguments or when the capture pipeline cannot
 *         be initialized.
 */
int capture_line(const char *command, const char *arg, char *out_buf, const size_t buf_size);

#endif /* CAPTURE_H */
