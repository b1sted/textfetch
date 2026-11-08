/* SPDX-License-Identifier: MIT */

#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#define _POSIX_C_SOURCE 200809L

#include "capture.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <spawn.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "sys_utils.h"

/* Extra bytes read after the first line to let short-lived commands exit. */
#define CAPTURE_DRAIN_LIMIT ((size_t)64 * 1024)

/**
 * Pointer to the process environment variables array.
 * Global POSIX variable for passing environment to child processes.
 */
extern char **environ;

/**
 * Truncates a captured buffer at the first line terminator.
 *
 * @param out_buf The buffer to sanitize.
 */
static void capture_strip_line(char *out_buf)
{
    for (size_t i = 0; out_buf[i] != '\0'; i++) {
        if (out_buf[i] == '\r' || out_buf[i] == '\n') {
            out_buf[i] = '\0';
            break;
        }
    }
}

void capture_job_init(capture_job_t *job)
{
    if (job == NULL) {
        return;
    }

    job->read_fd = -1;
    job->pid = -1;
}

int capture_start(capture_job_t *job, const char *command, const char *arg)
{
    int pipefd[2] = {-1, -1}; /* [0] - read, [1] - write */
    posix_spawn_file_actions_t actions = {0};
    bool actions_initialized = false;
    pid_t pid = -1;
    int spawn_status = 0;
    int result = -1;

    if (job == NULL || command == NULL) {
        return -1;
    }

    if (job->read_fd >= 0 || job->pid > 0) {
        return -1;
    }

    if (pipe(pipefd) != 0) {
        V_PRINTF("[ERROR] pipe failed: %s\n", strerror(errno));
        goto cleanup;
    }

    spawn_status = posix_spawn_file_actions_init(&actions);
    if (spawn_status != 0) {
        V_PRINTF("[ERROR] posix_spawn_file_actions_init failed: %s\n", strerror(spawn_status));
        goto cleanup;
    }
    actions_initialized = true;

    /* Keep only stdout; silence stdin and stderr as the Linux implementation does. */
    spawn_status =
        posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    if (spawn_status == 0) {
        spawn_status =
            posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    }
    if (spawn_status == 0) {
        spawn_status = posix_spawn_file_actions_addclose(&actions, pipefd[0]);
    }
    if (spawn_status == 0) {
        spawn_status = posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
    }
    if (spawn_status == 0) {
        spawn_status = posix_spawn_file_actions_addclose(&actions, pipefd[1]);
    }

    char *argv[] = {(char *)command, (char *)arg, (char *)NULL};

    if (spawn_status == 0) {
        spawn_status = posix_spawnp(&pid, command, &actions, NULL, argv, environ);
    }

    if (spawn_status != 0) {
        V_PRINTF("[ERROR] posix_spawn failed: %s\n", strerror(spawn_status));
        goto cleanup;
    }

    close(pipefd[1]);
    pipefd[1] = -1;

    job->read_fd = pipefd[0];
    job->pid = pid;
    pipefd[0] = -1;

    result = 0;

cleanup:
    if (actions_initialized) {
        posix_spawn_file_actions_destroy(&actions);
    }
    if (pipefd[0] != -1) {
        close(pipefd[0]);
    }
    if (pipefd[1] != -1) {
        close(pipefd[1]);
    }
    return result;
}

int capture_finish(capture_job_t *job, char *out_buf, const size_t buf_size)
{
    char discard[256] = {0};
    size_t drained = 0;
    size_t used = 0;
    pid_t waited = 0;
    int status = 0;

    if (job == NULL || job->read_fd < 0 || job->pid <= 0 || out_buf == NULL || buf_size == 0) {
        return -1;
    }

    out_buf[0] = '\0';

    bool line_done = (buf_size <= 1);

    while (drained < CAPTURE_DRAIN_LIMIT) {
        char *dst = line_done ? discard : out_buf + used;
        size_t capacity = line_done ? sizeof(discard) : buf_size - 1 - used;

        if (line_done && capacity > CAPTURE_DRAIN_LIMIT - drained) {
            capacity = CAPTURE_DRAIN_LIMIT - drained;
        }

        if (capacity == 0) {
            break;
        }

        ssize_t bytes_read = read(job->read_fd, dst, capacity);
        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (bytes_read == 0) {
            break;
        }

        if (line_done) {
            drained += (size_t)bytes_read;
            continue;
        }

        for (ssize_t i = 0; i < bytes_read; i++) {
            if (dst[i] == '\n') {
                used += (size_t)i;
                line_done = true;
                break;
            }
        }

        if (!line_done) {
            used += (size_t)bytes_read;
            if (used >= buf_size - 1) {
                used = buf_size - 1;
                line_done = true;
            }
        }

        out_buf[used] = '\0';
    }

    out_buf[used] = '\0';

    close(job->read_fd);
    job->read_fd = -1;

    do {
        waited = waitpid(job->pid, &status, 0);
    } while (waited == -1 && errno == EINTR);

    job->pid = -1;

    capture_strip_line(out_buf);

    if (waited == -1) {
        return 1;
    }

    return (WIFEXITED(status) && WEXITSTATUS(status) == 0 && out_buf[0] != '\0') ? 0 : 1;
}

int capture_line(const char *command, const char *arg, char *out_buf, const size_t buf_size)
{
    capture_job_t job;
    int result = -1;

    if (command == NULL || out_buf == NULL || buf_size == 0) {
        return -1;
    }

    out_buf[0] = '\0';

    capture_job_init(&job);
    result = capture_start(&job, command, arg);
    if (result != 0) {
        return result;
    }

    return capture_finish(&job, out_buf, buf_size);
}
