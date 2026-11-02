/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "capture.h"
#include "sys_utils.h"

/**
 * Pointer to the process environment variables array.
 * Global POSIX variable for passing environment to child processes.
 */
extern char **environ;

int capture_line(const char *command, const char *arg,
                 char *out_buf, const size_t buf_size) {
    if (!command || !out_buf || buf_size < 2) return -1;

    out_buf[0] = '\0';

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        V_PRINTF("[ERROR] pipe failed: %s\n", strerror(errno));
        return -1;
    }

    posix_spawn_file_actions_t actions;
    int spawn_status = posix_spawn_file_actions_init(&actions);
    if (spawn_status != 0) {
        V_PRINTF("[ERROR] posix_spawn_file_actions_init failed: %s\n",
                 strerror(spawn_status));
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    /* Keep only stdout; silence stdin and stderr as the Linux implementation does. */
    if (spawn_status == 0) {
        spawn_status = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO,
                                                        "/dev/null", O_RDONLY, 0);
    }
    if (spawn_status == 0) {
        spawn_status = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO,
                                                        "/dev/null", O_WRONLY, 0);
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

    char *argv[] = {(char *)command, (char *)arg, NULL};
    pid_t pid = 0;

    if (spawn_status == 0) {
        spawn_status = posix_spawnp(&pid, command, &actions, NULL, argv, environ);
    }

    posix_spawn_file_actions_destroy(&actions);

    if (spawn_status != 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        V_PRINTF("[ERROR] posix_spawn failed: %s\n", strerror(spawn_status));
        return -1;
    }

    close(pipefd[1]);

    FILE *stream = fdopen(pipefd[0], "r");
    if (stream) {
        if (fgets(out_buf, (int)buf_size, stream) != NULL) {
            out_buf[strcspn(out_buf, "\r\n")] = '\0';
        } else {
            V_PRINTF("[ERROR] failed to read command output for %s\n", command);
        }
        fclose(stream);
    } else {
        V_PRINTF("[ERROR] fdopen failed: %s\n", strerror(errno));
        close(pipefd[0]);
    }

    int wait_status = 0;
    pid_t waited;
    do {
        waited = waitpid(pid, &wait_status, 0);
    } while (waited == -1 && errno == EINTR);

    if (waited == -1) {
        V_PRINTF("[ERROR] waitpid failed: %s\n", strerror(errno));
        return 1;
    }

    return (WIFEXITED(wait_status) &&
            WEXITSTATUS(wait_status) == 0 &&
            out_buf[0] != '\0') ? 0 : 1;
}
