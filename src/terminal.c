/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <unistd.h>

#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "terminal.h"
#include "ui.h"

/**
 * Internal helper to identify and version the parent shell.
 * Reads the command name from /proc and executes it with '--version'
 * to capture the full version string via a pipe.
 * 
 * @param out_buf  The destination buffer for the shell version string.
 * @param buf_size The size of the destination buffer.
 */
static void term_get_shell(char *out_buf, const size_t buf_size);

void terminal_print_info(void) {
    char shell_buf[LINE_BUFFER] = {0};
    term_get_shell(shell_buf, LINE_BUFFER);

    char *locale = setlocale(LC_ALL, "");
    if (!locale) locale = "-";

    ui_print_info("Shell", shell_buf);
    ui_print_info("Locale", locale);
}

static void term_get_shell(char *out_buf, const size_t buf_size) {
    pid_t ppid = getppid();

    char proc_path[PATH_MAX] = {0};
    snprintf(proc_path, PATH_MAX, "/proc/%d/comm", ppid);

    FILE *fp = fopen(proc_path, "r");
    if (!fp) {
        V_PRINTF("Error: failed to open %s: %s\n", proc_path, strerror(errno));
        return ;
    }

    char comm_buf[LINE_BUFFER];
    if (fgets(comm_buf, LINE_BUFFER, fp) == NULL) {
        fclose(fp);
        return ;
    }

    fclose(fp);

    comm_buf[strcspn(comm_buf, "\n")] = 0;

    char bin_path[PATH_MAX] = "/usr/bin/";
    strncat(bin_path, comm_buf, PATH_MAX - strlen(bin_path) - 1);
    
    int pipefd[2]; // [0] - read, [1] - write

    if (pipe(pipefd) == -1) {
        V_PRINTF("Error: pipe failed: %s\n", strerror(errno));
        return ;
    }

    pid_t pid = fork();
    
    if (pid == -1) {
        V_PRINTF("Error: fork failed: %s\n", strerror(errno));
        return ;
    } else if (pid == 0) {
        close(pipefd[0]);

        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        execlp(bin_path, comm_buf, "--version", NULL);

        V_PRINTF("Error: exec failed for %s: %s\n", bin_path, strerror(errno));
        exit(1); 
    } else {
        close(pipefd[1]);
        
        FILE *pipe_stream = fdopen(pipefd[0], "r");
        if (!pipe_stream) {
            V_PRINTF("Error: fdopen failed: %s\n", strerror(errno));
            close(pipefd[0]);
            wait(NULL);
            return ;
        }

        if (fgets(out_buf, buf_size, pipe_stream) == NULL) {
            snprintf(out_buf, buf_size, "%s", comm_buf);  
        }

        size_t len = strlen(out_buf);
        if (len > 0 && out_buf[len - 1] == '\n') {
            out_buf[len - 1] = '\0';
        }

        char *p = strchr(out_buf, '(');
        if (p) {
            *p = '\0';
        }

        fclose(pipe_stream);
        wait(NULL);
    }
}
