/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include "capture.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/wait.h>

#include "sys_utils.h"

int capture_line(const char *command, const char *arg, char *out_buf, const size_t buf_size)
{
    int pipefd[2] = {-1, -1}; /* [0] - read, [1] - write */
    pid_t pid = -1;
    pid_t waited = 0;
    int status = 0;
    int result = -1;

    if (!command || !out_buf || buf_size == 0) {
        return -1;
    }

    out_buf[0] = '\0';

    if (pipe(pipefd) == -1) {
        V_PRINTF("[ERROR] pipe failed: %s\n", strerror(errno));
        goto cleanup;
    }

    pid = fork();
    if (pid == -1) {
        V_PRINTF("[ERROR] fork failed: %s\n", strerror(errno));
        goto cleanup;
    }

    if (pid == 0) {
        close(pipefd[0]);
        pipefd[0] = -1;

        int devnull = open("/dev/null", O_RDWR);
        if (devnull != -1) {
            if (dup2(devnull, STDIN_FILENO) == -1 || dup2(devnull, STDERR_FILENO) == -1) {
                _exit(1);
            }
            close(devnull);
        }

        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            _exit(1);
        }
        close(pipefd[1]);
        pipefd[1] = -1;

        if (arg) {
            execlp(command, command, arg, (char *)NULL);
        } else {
            execlp(command, command, (char *)NULL);
        }

        V_PRINTF("[ERROR] exec failed for %s: %s\n", command, strerror(errno));
        _exit(1);
    }

    close(pipefd[1]);
    pipefd[1] = -1;

    FILE *pipe_stream = fdopen(pipefd[0], "r");
    if (pipe_stream) {
        pipefd[0] = -1;

        size_t read_limit = buf_size;
        if (read_limit > (size_t)INT_MAX) {
            read_limit = (size_t)INT_MAX;
        }

        if (fgets(out_buf, (int)read_limit, pipe_stream) == NULL) {
            out_buf[0] = '\0';
        }

        if (fclose(pipe_stream) != 0) {
            V_PRINTF("[ERROR] fclose failed: %s\n", strerror(errno));
        }
    } else {
        V_PRINTF("[ERROR] fdopen failed: %s\n", strerror(errno));
        close(pipefd[0]);
        pipefd[0] = -1;
    }

    do {
        waited = waitpid(pid, &status, 0);
    } while (waited == -1 && errno == EINTR);

    for (size_t i = 0; i < buf_size && out_buf[i] != '\0'; i++) {
        if (out_buf[i] == '\r' || out_buf[i] == '\n') {
            out_buf[i] = '\0';
            break;
        }
    }

    if (waited == -1) {
        result = 1;
        goto cleanup;
    }

    result = (WIFEXITED(status) && WEXITSTATUS(status) == 0 && out_buf[0] != '\0') ? 0 : 1;

cleanup:
    if (pipefd[0] != -1) {
        close(pipefd[0]);
    }
    if (pipefd[1] != -1) {
        close(pipefd[1]);
    }
    return result;
}
