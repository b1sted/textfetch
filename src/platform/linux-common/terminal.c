/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include "pal/terminal_os.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <strings.h>
#include <unistd.h>

#include <sys/types.h>

#include "capture.h"
#include "defs.h"
#include "sys_utils.h"

/* Maximum number of parent processes inspected while looking for a shell. */
#define ANCESTOR_LIMIT 64

/* Shells that may own an interactive terminal. */
static const char *const shell_names[] = {"ash",     "bash", "csh",   "dash",       "elvish", "es",
                                          "fish",    "ksh",  "ksh93", "loksh",      "mksh",   "nu",
                                          "nushell", "oksh", "pdksh", "powershell", "pwsh",   "rc",
                                          "sh",      "tcsh", "xonsh", "yash",       "zsh",    NULL};

/**
 * Checks whether a process name belongs to a known shell.
 *
 * @param name Process or executable name.
 * @return true when the name matches a supported shell.
 */
static bool term_is_shell_name(const char *name)
{
    if (name == NULL || *name == '\0') {
        return false;
    }

    for (size_t i = 0; shell_names[i] != NULL; i++) {
        if (strcasecmp(name, shell_names[i]) == 0) {
            return true;
        }
    }

    return false;
}

/**
 * Reads the process name from /proc/<pid>/comm.
 *
 * Android restricts some /proc files even for processes that share a UID, but
 * the comm file has traditionally been readable and is the most portable way
 * to identify the parent shell.
 *
 * @param pid Process to inspect.
 * @param out_name Destination for the process name.
 * @param name_size Capacity of @p out_name.
 * @return true when the file was read and parsed.
 */
static bool term_read_comm(const pid_t pid, char *out_name, const size_t name_size)
{
    char path[PATH_BUFFER] = {0};
    char line[TINY_BUFFER] = {0};
    FILE *fp = NULL;
    bool ok = false;

    if (pid <= 0 || out_name == NULL || name_size == 0) {
        return false;
    }

    out_name[0] = '\0';

    if (snprintf(path, sizeof(path), "/proc/%d/comm", (int)pid) >= (int)sizeof(path)) {
        return false;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        return false;
    }

    if (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        snprintf(out_name, name_size, "%s", line);
        ok = (out_name[0] != '\0');
    }

    if (fclose(fp) != 0) {
        V_PRINTF("[ERROR] fclose %s failed: %s\n", path, strerror(errno));
    }

    return ok;
}

/**
 * Reads the process name and parent PID from /proc/<pid>/stat.
 *
 * @param pid Process to inspect.
 * @param out_name Destination for the process name (comm).
 * @param name_size Capacity of @p out_name.
 * @param out_ppid Destination for the parent PID.
 * @return true when the file was read and parsed.
 */
static bool term_read_process_stat(const pid_t pid, char *out_name, const size_t name_size,
                                   pid_t *out_ppid)
{
    char path[PATH_BUFFER] = {0};
    char line[LINE_BUFFER] = {0};
    FILE *fp = NULL;
    bool ok = false;

    if (pid <= 0 || out_name == NULL || name_size == 0 || out_ppid == NULL) {
        return false;
    }

    out_name[0] = '\0';
    *out_ppid = 0;

    if (snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid) >= (int)sizeof(path)) {
        return false;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        V_PRINTF("[ERROR] Failed to open %s: %s\n", path, strerror(errno));
        return false;
    }

    if (fgets(line, sizeof(line), fp) == NULL) {
        goto cleanup;
    }

    char *name_start = strchr(line, '(');
    char *name_end = strrchr(line, ')');
    if (name_start == NULL || name_end == NULL || name_end <= name_start) {
        goto cleanup;
    }

    size_t name_len = (size_t)(name_end - name_start - 1);
    if (name_len >= name_size) {
        name_len = name_size - 1;
    }
    memcpy(out_name, name_start + 1, name_len);
    out_name[name_len] = '\0';

    char *rest = name_end + 1;
    while (*rest == ' ') {
        rest++;
    }
    if (*rest == '\0') {
        goto cleanup;
    }

    /* Skip the process state, then read the parent PID. */
    rest++;
    char *end = NULL;
    long parent = strtol(rest, &end, 10);
    if (end == rest || parent < 0 || parent > (long)INT_MAX) {
        goto cleanup;
    }

    *out_ppid = (pid_t)parent;
    ok = true;

cleanup:
    if (fclose(fp) != 0) {
        V_PRINTF("[ERROR] fclose %s failed: %s\n", path, strerror(errno));
    }
    return ok;
}

/**
 * Reads the command line of a process and determines whether it is an
 * interactive shell invocation.
 *
 * Shells started by wrappers such as "sh -c ..." or "sh script.sh" are not
 * the terminal shell and must be skipped while walking the process tree.
 *
 * @param pid Process to inspect.
 * @param out_name Destination for the basename of argv[0].
 * @param name_size Capacity of @p out_name.
 * @param out_interactive Set to true when the command line looks interactive.
 * @return true when argv[0] was read.
 */
static bool term_read_cmdline(const pid_t pid, char *out_name, const size_t name_size,
                              bool *out_interactive)
{
    char path[PATH_BUFFER] = {0};
    char args[PATH_BUFFER] = {0};
    FILE *fp = NULL;
    bool ok = false;

    if (pid <= 0 || out_name == NULL || name_size == 0 || out_interactive == NULL) {
        return false;
    }

    out_name[0] = '\0';
    *out_interactive = true;

    if (snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid) >= (int)sizeof(path)) {
        return false;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return false;
    }

    size_t read_count = fread(args, 1, sizeof(args) - 1, fp);
    if (fclose(fp) != 0) {
        V_PRINTF("[ERROR] fclose %s failed: %s\n", path, strerror(errno));
    }
    if (read_count == 0) {
        return false;
    }
    args[read_count] = '\0';

    const char *arg0 = args;
    const char *basename = strrchr(arg0, '/');
    basename = basename ? basename + 1 : arg0;

    /* Login shells are prefixed with a dash, for example "-bash". */
    if (*basename == '-') {
        basename++;
    }
    if (*basename == '\0') {
        return false;
    }

    snprintf(out_name, name_size, "%s", basename);

    size_t offset = strlen(arg0) + 1;
    bool first_arg = true;
    while (offset < read_count) {
        const char *arg = arg0 + offset;
        if (*arg == '\0') {
            break;
        }

        if (strcmp(arg, "-c") == 0 || (first_arg && arg[0] != '-')) {
            *out_interactive = false;
            break;
        }

        first_arg = false;
        offset += strlen(arg) + 1;
    }

    ok = true;
    return ok;
}

/**
 * Resolves the executable path of a process.
 *
 * @param pid Process to inspect.
 * @param out_path Destination for the resolved path.
 * @param path_size Capacity of @p out_path.
 * @return true when the link was read.
 */
static bool term_read_exe(const pid_t pid, char *out_path, const size_t path_size)
{
    char path[PATH_BUFFER] = {0};
    ssize_t length = -1;

    if (pid <= 0 || out_path == NULL || path_size == 0) {
        return false;
    }

    out_path[0] = '\0';

    if (snprintf(path, sizeof(path), "/proc/%d/exe", (int)pid) >= (int)sizeof(path)) {
        return false;
    }

    length = readlink(path, out_path, path_size - 1);
    if (length <= 0) {
        return false;
    }

    out_path[length] = '\0';
    return true;
}

/* Deferred shell discovery used to overlap the version probe with other work. */
typedef struct {
    capture_job_t job;
    char shell_name[TINY_BUFFER];
    char exe_path[PATH_BUFFER];
    bool have_exe;
    bool pending;
} term_shell_state_t;

static term_shell_state_t shell_state = {0};

/**
 * Walks the parent process chain and returns the first interactive shell.
 *
 * If the chain cannot be followed because the process information is
 * restricted, the nearest shell is returned as a best-effort result.
 *
 * @param out_pid Destination for the shell PID.
 * @param out_name Destination for the shell name.
 * @param name_size Capacity of @p out_name.
 * @return true when a shell was found.
 */
static bool term_find_shell(pid_t *out_pid, char *out_name, const size_t name_size)
{
    if (out_pid == NULL || out_name == NULL || name_size == 0) {
        return false;
    }

    pid_t pid = getppid();

    for (size_t depth = 0; depth < ANCESTOR_LIMIT && pid > 0; depth++) {
        char comm_buf[TINY_BUFFER] = {0};
        char cmdline_name[TINY_BUFFER] = {0};
        const char *shell_name = NULL;
        pid_t ppid = 0;
        bool interactive = true;
        bool have_stat = term_read_process_stat(pid, comm_buf, sizeof(comm_buf), &ppid);

        if (!have_stat && !term_read_comm(pid, comm_buf, sizeof(comm_buf))) {
            break;
        }

        if (!term_read_cmdline(pid, cmdline_name, sizeof(cmdline_name), &interactive)) {
            cmdline_name[0] = '\0';
        }

        if (term_is_shell_name(cmdline_name)) {
            shell_name = cmdline_name;
        } else if (term_is_shell_name(comm_buf)) {
            shell_name = comm_buf;
        }

        /*
         * Wrappers such as "sh -c ..." or "sh script.sh" are shells too, but
         * they are not attached to the terminal the user launched us from.
         * Keep walking until an interactive shell (or the root) is reached.
         */
        if (shell_name != NULL && interactive) {
            snprintf(out_name, name_size, "%s", shell_name);
            *out_pid = pid;
            return true;
        }

        /*
         * Without the parent PID the chain cannot be followed any further.
         * Report the nearest shell even if it looks like a wrapper, since it
         * is still a better answer than the environment fallback.
         */
        if (!have_stat) {
            if (shell_name != NULL) {
                snprintf(out_name, name_size, "%s", shell_name);
                *out_pid = pid;
                return true;
            }
            break;
        }

        pid = ppid;
    }

    return false;
}

/**
 * Produces a display string for the detected shell.
 *
 * The version is obtained by running the shell with --version, first through
 * its name and then through the resolved executable. If both fail, the shell
 * name alone is used as a stable fallback.
 *
 * @param pid PID of the detected shell.
 * @param shell_name Name used to identify the shell.
 * @param out_buf Destination for the formatted shell string.
 * @param buf_size Capacity of @p out_buf.
 */
static void term_describe_shell(const pid_t pid, const char *shell_name, char *out_buf,
                                const size_t buf_size)
{
    char exe_path[PATH_BUFFER] = {0};
    char version_buf[LINE_BUFFER] = {0};

    if (shell_name == NULL || out_buf == NULL || buf_size == 0) {
        return;
    }

    bool captured = (capture_line(shell_name, "--version", version_buf, sizeof(version_buf)) == 0);

    if (!captured && term_read_exe(pid, exe_path, sizeof(exe_path))) {
        captured = (capture_line(exe_path, "--version", version_buf, sizeof(version_buf)) == 0);
    }
    if (captured) {
        term_sanitize_name(version_buf);
        if (version_buf[0] != '\0') {
            snprintf(out_buf, buf_size, "%s", version_buf);
            return;
        }
    }

    snprintf(out_buf, buf_size, "%s", shell_name);
}

void term_prepare_shell(void)
{
    pid_t pid = 0;
    char shell_name[TINY_BUFFER] = {0};

    if (shell_state.pending) {
        return;
    }

    if (!term_find_shell(&pid, shell_name, sizeof(shell_name))) {
        V_PRINTF("[WARNING] shell was not found in the process tree\n");
        return;
    }

    capture_job_init(&shell_state.job);
    if (capture_start(&shell_state.job, shell_name, "--version") != 0) {
        V_PRINTF("[WARNING] failed to start the shell version probe for %s\n", shell_name);
        return;
    }

    snprintf(shell_state.shell_name, sizeof(shell_state.shell_name), "%s", shell_name);
    shell_state.have_exe = term_read_exe(pid, shell_state.exe_path, sizeof(shell_state.exe_path));
    shell_state.pending = true;
}

void term_get_shell(char *out_buf, const size_t buf_size)
{
    if (out_buf == NULL || buf_size == 0) {
        return;
    }

    term_fallback_shell(out_buf, buf_size);

    if (shell_state.pending) {
        char version_buf[LINE_BUFFER] = {0};
        char retry_buf[LINE_BUFFER] = {0};
        const char *resolved = NULL;
        bool have_exe = shell_state.have_exe;
        int result = 0;

        shell_state.pending = false;
        result = capture_finish(&shell_state.job, version_buf, sizeof(version_buf));

        if (result == 0) {
            term_sanitize_name(version_buf);
            if (version_buf[0] != '\0') {
                resolved = version_buf;
            }
        }

        if (resolved == NULL) {
            V_PRINTF("[WARNING] failed to capture the version of %s\n", shell_state.shell_name);

            if (have_exe && capture_line(shell_state.exe_path, "--version", retry_buf,
                                         sizeof(retry_buf)) == 0) {
                term_sanitize_name(retry_buf);
                if (retry_buf[0] != '\0') {
                    resolved = retry_buf;
                }
            }
        }

        snprintf(out_buf, buf_size, "%s", (resolved != NULL) ? resolved : shell_state.shell_name);

        shell_state.have_exe = false;
        shell_state.exe_path[0] = '\0';
        shell_state.shell_name[0] = '\0';
        return;
    }

    pid_t pid = 0;
    char shell_name[TINY_BUFFER] = {0};

    if (term_find_shell(&pid, shell_name, sizeof(shell_name))) {
        term_describe_shell(pid, shell_name, out_buf, buf_size);
    }
}
