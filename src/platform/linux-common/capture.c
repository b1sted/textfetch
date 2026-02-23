/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include <errno.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/wait.h>

#include "capture.h"
#include "sys_utils.h"

int capture_line(const char *command, const char *arg,
                 char *out_buf, const size_t buf_size) {
    if (!command || !out_buf || buf_size == 0) return -1;

    out_buf[0] = '\0';

    int pipefd[2]; /* [0] - read, [1] - write */

    if (pipe(pipefd) == -1) {
        V_PRINTF("Error: pipe failed: %s\n", strerror(errno));
        return -1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        V_PRINTF("Error: fork failed: %s\n", strerror(errno));
        return -1;
    } else if (pid == 0) {
        close(pipefd[0]);

        int devnull = open("/dev/null", O_RDWR);
        if (devnull != -1) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        if (arg) {
            execlp(command, command, arg, (char *)NULL);
        } else {
            execlp(command, command, (char *)NULL);
        }

        V_PRINTF("Error: exec failed for %s: %s\n", command, strerror(errno));
        _exit(1);
    } else {
        close(pipefd[1]);

        FILE *pipe_stream = fdopen(pipefd[0], "r");
        if (pipe_stream) {
            fgets(out_buf, (int)buf_size, pipe_stream);
            fclose(pipe_stream);
        } else {
            V_PRINTF("Error: fdopen failed: %s\n", strerror(errno));
            close(pipefd[0]);
        }

        int status;
        waitpid(pid, &status, 0);

        out_buf[strcspn(out_buf, "\r\n")] = '\0';

        return (WIFEXITED(status) &&
                WEXITSTATUS(status) == 0 &&
                out_buf[0] != '\0') ? 0 : 1;
    }
}