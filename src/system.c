/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h> 

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <unistd.h>

#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "system.h"
#include "ui.h"

static struct utsname sys_info;
static struct sysinfo sys_stat;

/**
 * Internal helper to retrieve the effective username.
 * Queries the system password database (getpwuid) using the current 
 * effective user ID. Falls back to "unknown" on failure.
 *
 * @param out_buf  Destination buffer for the username.
 * @param buf_size Maximum size of the destination buffer.
 */
static void sys_get_identity(char *out_buf, const size_t buf_size);

/**
 * Internal helper to identify the OS distribution name.
 * Parses /etc/os-release (or /usr/lib/os-release) to find NAME or 
 * PRETTY_NAME keys, then appends the machine architecture.
 *
 * @param out_buf  Destination buffer for the distro string.
 * @param buf_size Maximum size of the destination buffer.
 */
static void sys_get_distro(char *out_buf, const size_t buf_size);

/**
 * Formats the raw uptime value into a human-readable duration string.
 * Result format: "D days, HH:MM:SS" or "HH:MM:SS".
 * Operates on a local copy of the uptime to preserve the original value.
 *
 * @param out_buffer Destination buffer for the formatted string.
 * @param buf_size   Maximum size of the destination buffer.
 */
static void sys_format_uptime(char *out_buf, const size_t buf_size);

void system_init(void) {
    const char *fallback = "unknown";

    memset(&sys_info, 0, sizeof(struct utsname));

    if (uname(&sys_info) != 0) {
        V_PRINTF("Error: uname failed: %s\n", strerror(errno));

        strncpy(sys_info.sysname,  fallback, sizeof(sys_info.sysname)  - 1);
        strncpy(sys_info.nodename, fallback, sizeof(sys_info.nodename) - 1);
        strncpy(sys_info.release,  fallback, sizeof(sys_info.release)  - 1);
        strncpy(sys_info.machine,  fallback, sizeof(sys_info.machine)  - 1);
    }

    memset(&sys_stat, 0, sizeof(struct sysinfo));

    if (sysinfo(&sys_stat) != 0) {
        V_PRINTF("Error: sysinfo failed: %s\n", strerror(errno));

        sys_stat.uptime = 0;
        sys_stat.procs = 0;
    }
}

void system_print_header(void) {
    char username[LOGIN_NAME_MAX] = {0};

    sys_get_identity(username, LOGIN_NAME_MAX);
    ui_render_header(username, sys_info.nodename);
}

void system_print_info(void) {
    char os_buf[LINE_BUFFER] = {0};
    sys_get_distro(os_buf, LINE_BUFFER);
    
    char uptime_buf[LINE_BUFFER] = {0};
    sys_format_uptime(uptime_buf, LINE_BUFFER);

    char procs_buf[SMALL_BUFFER] = {0};
    snprintf(procs_buf, SMALL_BUFFER, "%hu", sys_stat.procs);

    ui_print_info("OS", os_buf);
    ui_print_info("Kernel", sys_info.release);
    ui_print_info("Uptime", uptime_buf);
    ui_print_info("Processes", procs_buf);
}

static void sys_get_identity(char *out_buf, const size_t buf_size) {
    struct passwd *pwd;
    uid_t uid = geteuid();

    if ((pwd = getpwuid(uid)) == NULL) {
        V_PRINTF("Error: getpwuid failed: %s\n", strerror(errno));
        snprintf(out_buf, buf_size, "unknown");
        return ;
    }

    snprintf(out_buf, buf_size, "%s", pwd->pw_name);
}

static void sys_get_distro(char *out_buf, const size_t buf_size) {
    FILE *fp = fopen("/etc/os-release", "r");
    if (!fp) {
        fp = fopen("/usr/lib/os-release", "r");

        if (!fp) {
            V_PRINTF("Error: open os-release file failed: %s\n", strerror(errno));
            snprintf(out_buf, buf_size, "%s %s", sys_info.sysname, sys_info.machine);
            return ;
        }
    }

    char line[LINE_BUFFER];
    while (fgets(line, sizeof(line), fp)) {
        char *delim = strchr(line, '=');
        if (!delim) continue;

        *delim = 0;
        char *key = line;
        char *value = delim + 1;

        value[strcspn(value, "\n")] = 0;

        if (value[0] == '"') {
            value++;

            size_t len = strlen(value);

            if (len > 0 && value[len - 1] == '"') {
                value[len - 1] = 0;
            }
        }

        if (strcmp(key, "NAME") == 0) {
            snprintf(out_buf, buf_size, "%s", value);
        }

        if (strcmp(key, "PRETTY_NAME") == 0) {
            snprintf(out_buf, buf_size, "%s", value);
            break;
        }
    }

    fclose(fp);

    size_t len = strlen(out_buf);
    snprintf(out_buf + len, buf_size - len, " %s", sys_info.machine);
}

static void sys_format_uptime(char *out_buf, const size_t buf_size) {
    long uptime = sys_stat.uptime;

    long days = uptime / 86400;
    uptime %= 86400;

    long hours = uptime / 3600;
    uptime %= 3600;
    
    long minutes = uptime / 60;
    long seconds = uptime % 60;

    if (days != 0) {
        snprintf(out_buf, buf_size, "%ld days, %02ld:%02ld:%02ld", days, hours, minutes, seconds);
    } else {
        snprintf(out_buf, buf_size, "%02ld:%02ld:%02ld", hours, minutes, seconds);
    }
}
