/* SPDX-License-Identifier: MIT */

#include "pal/terminal_os.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include <sys/types.h>

#include <libproc.h>

#include "capture.h"
#include "defs.h"
#include "sys_utils.h"

/* Deferred shell discovery used to overlap the version probe with other work. */
typedef struct {
    capture_job_t job;
    bool pending;
} term_shell_state_t;

static term_shell_state_t shell_state = {0};

void term_prepare_shell(void)
{
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE] = {0};
    pid_t ppid = 0;
    int ret = 0;

    if (shell_state.pending) {
        return;
    }

    ppid = getppid();
    ret = proc_pidpath(ppid, pathbuf, PROC_PIDPATHINFO_MAXSIZE);

    if (ret <= 0) {
        V_PRINTF("[ERROR] proc_pidpath(pid: %d) failed: %s\n", (int)ppid, strerror(errno));
        return;
    }

    capture_job_init(&shell_state.job);
    if (capture_start(&shell_state.job, pathbuf, "--version") != 0) {
        return;
    }

    shell_state.pending = true;
}

void term_get_shell(char *out_buf, const size_t buf_size)
{
    if (!out_buf || buf_size == 0) {
        return;
    }

    pid_t ppid = getppid();

    term_fallback_shell(out_buf, buf_size);

    if (shell_state.pending) {
        char shell_buf[LINE_BUFFER] = {0};

        shell_state.pending = false;

        if (capture_finish(&shell_state.job, shell_buf, sizeof(shell_buf)) == 0) {
            term_sanitize_name(shell_buf);

            if (shell_buf[0] != '\0') {
                snprintf(out_buf, buf_size, "%s", shell_buf);
            }
        }

        return;
    }

    char pathbuf[PROC_PIDPATHINFO_MAXSIZE] = {0};
    int ret = proc_pidpath(ppid, pathbuf, PROC_PIDPATHINFO_MAXSIZE);

    if (ret <= 0) {
        V_PRINTF("[ERROR] proc_pidpath(pid: %d) failed: %s\n", (int)ppid, strerror(errno));
        return;
    }

    char shell_buf[LINE_BUFFER] = {0};
    if (capture_line(pathbuf, "--version", shell_buf, sizeof(shell_buf)) != 0) {
        V_PRINTF("[ERROR] Failed to capture shell version from comm: %s\n"
                 "        Using a shell from environment\n",
                 pathbuf);
        return;
    }

    term_sanitize_name(shell_buf);

    snprintf(out_buf, buf_size, "%s", shell_buf);
}
