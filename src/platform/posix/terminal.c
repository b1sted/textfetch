/* SPDX-License-Identifier: MIT */

#include "pal/terminal_os.h"

#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "defs.h"
#include "sys_utils.h"
#include "terminal.h"
#include "ui.h"

#ifdef __APPLE__
/**
 * Reads the current macOS locale through CoreFoundation.
 *
 * CoreFoundation reports identifiers such as "en_US" without an encoding, so
 * UTF-8 is appended when the identifier does not already carry one. This is
 * the fallback for sessions started from the graphical environment, where the
 * shell locale variables are usually absent and setlocale() returns "C".
 *
 * @param out_buf Destination for the formatted locale name.
 * @param buf_size Capacity of @p out_buf.
 * @return true when the locale was obtained.
 */
static bool term_read_system_locale(char *out_buf, const size_t buf_size)
{
    CFLocaleRef locale = NULL;
    CFStringRef identifier = NULL;
    char raw[LINE_BUFFER] = {0};
    bool ok = false;

    if (out_buf == NULL || buf_size == 0) {
        return false;
    }

    out_buf[0] = '\0';

    locale = CFLocaleCopyCurrent();
    if (locale == NULL) {
        return false;
    }

    identifier = CFLocaleGetIdentifier(locale);
    if (identifier != NULL &&
        CFStringGetCString(identifier, raw, (CFIndex)sizeof(raw), kCFStringEncodingUTF8)) {
        if (strcmp(raw, "C") == 0 || strcmp(raw, "POSIX") == 0 || strchr(raw, '.') != NULL) {
            snprintf(out_buf, buf_size, "%s", raw);
        } else {
            snprintf(out_buf, buf_size, "%s.UTF-8", raw);
        }
        ok = (out_buf[0] != '\0');
    }

    CFRelease(locale);

    return ok;
}
#endif

void term_print_info(void)
{
    char shell_buf[LINE_BUFFER] = {0};
    const char *locale = NULL;

#ifdef __APPLE__
    char locale_buf[LINE_BUFFER] = {0};
#endif

    term_get_shell(shell_buf, LINE_BUFFER);

    locale = getenv("LC_ALL");
    if (locale == NULL || *locale == '\0') {
        locale = getenv("LC_CTYPE");
    }
    if (locale == NULL || *locale == '\0') {
        locale = getenv("LANG");
    }

#ifdef __APPLE__
    if ((locale == NULL || *locale == '\0') &&
        term_read_system_locale(locale_buf, sizeof(locale_buf))) {
        locale = locale_buf;
    }
#endif

    if (locale == NULL || *locale == '\0') {
        locale = setlocale(LC_CTYPE, "");
        if (locale == NULL || *locale == '\0') {
            V_PRINTF("[ERROR] setlocale(LC_CTYPE, \"\") failed\n");
            locale = "-";
        }
    }

    ui_print_info("Shell", shell_buf);
    ui_print_info("Locale", locale);
}

void term_fallback_shell(char *out_buf, const size_t buf_size)
{
    if (!out_buf || buf_size == 0) {
        return;
    }

    const char *env_shell = getenv("SHELL");

    if (!env_shell || *env_shell == '\0') {
        snprintf(out_buf, buf_size, "sh");
        return;
    }

    const char *last_slash = strrchr(env_shell, '/');

    const char *name = last_slash ? (last_slash + 1) : env_shell;

    snprintf(out_buf, buf_size, "%s", name);
}

void term_sanitize_name(char *out_buf)
{
    if (!out_buf || !*out_buf) {
        return;
    }

    const char *gnu_prefix = "GNU ";
    size_t prefix_len = strlen(gnu_prefix);
    if (strncmp(out_buf, gnu_prefix, prefix_len) == 0) {
        memmove(out_buf, out_buf + prefix_len, strlen(out_buf) - prefix_len + 1);
    }

    const char *version_marker = ", version";
    char *version_ptr = strstr(out_buf, version_marker);
    if (version_ptr) {
        size_t marker_len = strlen(version_marker);
        memmove(version_ptr, version_ptr + marker_len, strlen(version_ptr + marker_len) + 1);
    }

    char *version_start = strpbrk(out_buf, "0123456789");
    char *name_end = strchr(out_buf, ' ');

    if (version_start && name_end && name_end < version_start) {
        size_t name_len = (size_t)(name_end - out_buf);
        memmove(out_buf + name_len + 1, version_start, strlen(version_start) + 1);
        out_buf[name_len] = ' ';
    }

    version_start = strpbrk(out_buf, "0123456789");
    if (version_start) {
        char *garbage = strpbrk(version_start, " (");
        if (garbage) {
            *garbage = '\0';
        }
    }
}
