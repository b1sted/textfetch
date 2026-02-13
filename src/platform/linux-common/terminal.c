/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
#include "internal/terminal_os.h"
#include "ui.h"
#include "capture.h"

void term_get_shell(char *out_buf, const size_t buf_size) {
    if (!out_buf || buf_size == 0) return;

    term_fallback_shell(out_buf, buf_size);

    pid_t ppid = getppid();

    char proc_path[PATH_BUFFER] = {0};
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/comm", ppid);

    FILE *fp = fopen(proc_path, "r");
    if (!fp) {
        V_PRINTF("[ERROR] Failed to open %s: %s\n", proc_path, strerror(errno));
        return;
    }

    char comm_buf[TINY_BUFFER];
    if (fgets(comm_buf, sizeof(comm_buf), fp) == NULL) {
        fclose(fp);
        return;
    }

    fclose(fp);

    comm_buf[strcspn(comm_buf, "\n")] = 0;

    char shell_buf[LINE_BUFFER] = {0};
    if (capture_line(comm_buf, "--version", shell_buf, sizeof(shell_buf)) != 0) {
        V_PRINTF("[ERROR] Failed to capture shell version from comm: %s\n"
                 "        Using a shell from environment\n", comm_buf);
        return;
    }

    term_sanitize_name(shell_buf);

    snprintf(out_buf, buf_size, "%s", shell_buf);
}
