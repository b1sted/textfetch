/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <locale.h>

#include "defs.h"
#include "sys_utils.h"
#include "ui.h"

#include "pal/terminal_os.h"

void terminal_print_info(void) {
    char shell_buf[LINE_BUFFER] = {0};
    term_get_shell(shell_buf, LINE_BUFFER);

    char *locale = setlocale(LC_ALL, "");
    if (!locale) {
        V_PRINTF("[ERROR] setlocale(LC_ALL, \"\") failed\n");
        locale = "-";
    }

    ui_print_info("Shell", shell_buf);
    ui_print_info("Locale", locale);
}

void term_fallback_shell(char *out_buf, const size_t buf_size) {
    if (!out_buf || buf_size == 0) return;

    const char *env_shell = getenv("SHELL");

    if (!env_shell || *env_shell == '\0') {
        snprintf(out_buf, buf_size, "sh");
        return;
    }

    const char *last_slash = strrchr(env_shell, '/');

    const char *name = last_slash ? (last_slash + 1) : env_shell;

    snprintf(out_buf, buf_size, "%s", name);
}

void term_sanitize_name(char *out_buf) {
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