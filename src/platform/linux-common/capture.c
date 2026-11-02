/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/wait.h>

#include "capture.h"
#include "sys_utils.h"

int capture_line(const char *command, const char *arg, char *out_buf, const size_t buf_size)
{
    if (!command || !out_buf || buf_size == 0) {
        return -1;
    }

    out_buf[0] = '\0';

    int pipefd[2]; /* [0] - read, [1] - write */

    if (pipe(pipefd) == -1) {
        V_PRINTF("[ERROR] pipe failed: %s\n", strerror(errno));
        return -1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        V_PRINTF("[ERROR] fork failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    } else if (pid == 0) {
        close(pipefd[0]);

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

        if (arg) {
            execlp(command, command, arg, (char *)NULL);
        } else {
            execlp(command, command, (char *)NULL);
        }

        V_PRINTF("[ERROR] exec failed for %s: %s\n", command, strerror(errno));
        _exit(1);
    } else {
        close(pipefd[1]);

        FILE *pipe_stream = fdopen(pipefd[0], "r");
        if (pipe_stream) {
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
        }

        int status = 0;
        pid_t waited = 0;

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
            return 1;
        }

        return (WIFEXITED(status) && WEXITSTATUS(status) == 0 && out_buf[0] != '\0') ? 0 : 1;
    }
}