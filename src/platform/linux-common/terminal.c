/* SPDX-License-Identifier: MIT */

#include "pal/terminal_os.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include <sys/types.h>

#include "capture.h"
#include "defs.h"
#include "sys_utils.h"

void term_get_shell(char *out_buf, const size_t buf_size)
{
    FILE *fp = NULL;
    char proc_path[PATH_BUFFER] = {0};
    char comm_buf[TINY_BUFFER] = {0};
    char shell_buf[LINE_BUFFER] = {0};
    pid_t ppid = 0;

    if (!out_buf || buf_size == 0) {
        goto cleanup;
    }

    term_fallback_shell(out_buf, buf_size);

    ppid = getppid();
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/comm", (int)ppid);

    fp = fopen(proc_path, "r");
    if (!fp) {
        V_PRINTF("[ERROR] Failed to open %s: %s\n", proc_path, strerror(errno));
        goto cleanup;
    }

    if (fgets(comm_buf, sizeof(comm_buf), fp) == NULL) {
        goto cleanup;
    }

    fclose(fp);
    fp = NULL;

    comm_buf[strcspn(comm_buf, "\n")] = '\0';

    if (capture_line(comm_buf, "--version", shell_buf, sizeof(shell_buf)) != 0) {
        V_PRINTF("[ERROR] Failed to capture shell version from comm: %s\n"
                 "        Using a shell from environment\n",
                 comm_buf);
        goto cleanup;
    }

    term_sanitize_name(shell_buf);
    snprintf(out_buf, buf_size, "%s", shell_buf);

cleanup:
    if (fp) {
        fclose(fp);
    }
}
