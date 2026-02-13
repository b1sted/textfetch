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

int capture_line(const char *command, const char *arg, char *out_buf, const size_t buf_size) {
    if (!command || !out_buf || buf_size < 2) return -1;

    out_buf[0] = '\0';

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        V_PRINTF("[Error] pipe failed: %s\n", strerror(errno));
        return -1;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

    posix_spawn_file_actions_addclose(&actions, pipefd[0]);
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipefd[1]);

    char *argv[] = {(char *)command, (char *)arg, NULL};
    pid_t pid;
    
    int spawn_status = posix_spawnp(&pid, command, &actions, NULL, argv, environ);

    int result = -1;

    if (spawn_status == 0) {
        close(pipefd[1]);

        FILE *stream = fdopen(pipefd[0], "r");
        if (stream) {
            if (fgets(out_buf, (int)buf_size, stream) != NULL) {
                out_buf[strcspn(out_buf, "\r\n")] = '\0';
            } else {
                V_PRINTF("[Error] failed to read command output for %s\n", command);
            }
            fclose(stream);
        } else {
            close(pipefd[0]);
        }

        int wait_status;
        waitpid(pid, &wait_status, 0);

        if (WIFEXITED(wait_status) && WEXITSTATUS(wait_status) == 0 && out_buf[0] != '\0') {
            result = 0;
        }
        
    } else {
        close(pipefd[0]);
        close(pipefd[1]);
        V_PRINTF("[Error] posix_spawn failed: %s\n", strerror(spawn_status));
    }

    posix_spawn_file_actions_destroy(&actions);
    
    return result; 
}
