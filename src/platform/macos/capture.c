/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <spawn.h>
#include <unistd.h>

#include <sys/wait.h>

#include "capture.h"
#include "ui.h"

/**
 * Pointer to the process environment variables array.
 * Global POSIX variable for passing environment to child processes.
 */
extern char **environ;

int capture_line(const char *command, char *out_buf, const size_t buf_size) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        V_PRINTF("Error: pipe failed: %s\n", strerror(errno));
        return -1;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

    posix_spawn_file_actions_addclose(&actions, pipefd[0]);
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipefd[1]);

    char *argv[] = {"sh", "-c", (char *)command, NULL};
    pid_t pid;
    int status = posix_spawnp(&pid, "sh", &actions, NULL, argv, environ);

    if (status == 0) {
        close(pipefd[1]);

        FILE *stream = fdopen(pipefd[0], "r");
        if (stream) {
            if (fgets(out_buf, (int)buf_size, stream) == NULL) {
                V_PRINTF("Error: failed to read command output\n");
            }

            fclose(stream); // Also closes pipefd[0]
        } else {
            close(pipefd[0]);
        }

        out_buf[strcspn(out_buf, "\r\n")] = '\0';
        waitpid(pid, NULL, 0);
    } else {
        close(pipefd[0]);
        close(pipefd[1]);
        V_PRINTF("Error: posix_spawn failed: %s\n", strerror(status));
    }

    posix_spawn_file_actions_destroy(&actions);
    return status;
}
