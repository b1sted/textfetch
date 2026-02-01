/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <spawn.h>
#include <unistd.h>

#include <sys/types.h>   /* Must be included before libproc.h */
#include <libproc.h>

#include <sys/utsname.h>
#include <sys/wait.h>

#include "terminal.h"
#include "ui.h"

/**
 * Pointer to the process environment variables array.
 * Global POSIX variable for passing environment to child processes.
 */
extern char **environ;

/**
 * Internal helper to identify the parent shell and its version.
 * Obtains the parent process path via libproc and executes it with
 * the '--version' flag to capture the output.
 * 
 * @param out_buf  The destination buffer for the shell information.
 * @param buf_size The size of the destination buffer.
 */
static void term_get_shell(char *out_buf, const size_t buf_size);

/**
 * Executes a command and captures its first line of standard output.
 * Uses posix_spawn for efficient process creation on Darwin systems.
 *
 * @param command  The full command string to execute.
 * @param out_buf  Buffer to store the captured output.
 * @param buf_size Size of the output buffer.
 * @return 0 on success, non-zero on error.
 */
static int term_exec_capture(const char *command, char *out_buf, 
                             const size_t buf_size);

/**
 * Cleans up the shell version string by removing redundant prefixes,
 * intermediate noise (like (GDB)), and trailing architecture info.
 * 
 * Example: "GNU gdb (GDB) 14.1" -> "gdb 14.1"
 * Example: "zsh 5.9 (x86_64-apple-darwin23.0)" -> "zsh 5.9"
 * 
 * @param out_buf  The buffer containing the raw string to sanitize.
 */
static void term_sanitize_name(char *out_buf);

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
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE] = {0};

    const char *fallback_shell = getenv("SHELL");
    if (!fallback_shell) fallback_shell = "unknown";

    int ret = proc_pidpath(ppid, pathbuf, PROC_PIDPATHINFO_MAXSIZE);

    if (ret <= 0) {
        V_PRINTF("Error: could not obtain path to parent process via libproc\n");
        snprintf(out_buf, buf_size, "%s", fallback_shell);
        return;
    }

    size_t len = strlen(pathbuf);
    snprintf(pathbuf + len, sizeof(pathbuf) - len, " --version");

    if (term_exec_capture(pathbuf, out_buf, buf_size) != 0) {
        snprintf(out_buf, buf_size, "%s", fallback_shell);
        return;
    }

    term_sanitize_name(out_buf);
}

static int term_exec_capture(const char *command, char *out_buf, const size_t buf_size) {
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

static void term_sanitize_name(char *out_buf) {
    if (!out_buf || !*out_buf) return;

    char *gnu_prefix = "GNU ";
    size_t prefix_len = strlen(gnu_prefix);
    if (strncmp(out_buf, gnu_prefix, prefix_len) == 0) {
        memmove(out_buf, out_buf + prefix_len, strlen(out_buf) - prefix_len + 1);
    }

    char *version_marker = ", version";
    char *version_ptr = strstr(out_buf, version_marker);
    if (version_ptr) {
        size_t marker_len = strlen(version_marker);
        memmove(version_ptr, version_ptr + marker_len, strlen(version_ptr + marker_len) + 1);
    }

    char *version_start = strpbrk(out_buf, "0123456789");
    char *name_end = strchr(out_buf, ' ');

    if (version_start && name_end && name_end < version_start) {
        size_t name_len = name_end - out_buf;
        memmove(out_buf + name_len + 1, version_start, strlen(version_start) + 1);
        out_buf[name_len] = ' ';
    }

    version_start = strpbrk(out_buf, "0123456789");
    if (version_start) {
        char *garbage = strpbrk(version_start, " (");
        if (garbage) *garbage = '\0';
    }
}