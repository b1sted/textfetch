/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <string.h>

#include <errno.h>

#include <unistd.h>

#include <sys/types.h>

#include <libproc.h>

#include "capture.h"
#include "defs.h"
#include "sys_utils.h"

#include "pal/terminal_os.h"

void term_get_shell(char *out_buf, const size_t buf_size) {
    pid_t ppid = getppid();

    term_fallback_shell(out_buf, buf_size);

    char pathbuf[PROC_PIDPATHINFO_MAXSIZE] = {0};
    int ret = proc_pidpath(ppid, pathbuf, PROC_PIDPATHINFO_MAXSIZE);

    if (ret <= 0) {
        V_PRINTF("[ERROR] proc_pidpath(pid: %d) failed: %s\n", ppid, strerror(errno));
        return;
    }

    char shell_buf[LINE_BUFFER] = {0};
    if (capture_line(pathbuf, "--version", shell_buf, sizeof(shell_buf)) != 0) {
        V_PRINTF("[ERROR] Failed to capture shell version from comm: %s\n"
                 "        Using a shell from environment\n", pathbuf);
        return;
    }

    term_sanitize_name(shell_buf);

    snprintf(out_buf, buf_size, "%s", shell_buf);
}
