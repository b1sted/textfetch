/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h> 

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <spawn.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "system.h"
#include "ui.h"

/**
 * Mapping structure to associate Darwin kernel major versions 
 * with macOS marketing/release names.
 */
typedef struct {
    int major;
    const char *name;
} macos_name_map_t;

static struct utsname sys_info;

/**
 * Darwin major version map.
 * Darwin version = macOS version + 9 (for macOS 11+).
 * Darwin version = macOS minor + 4 (for Mac OS X 10.x).
 */
static const macos_name_map_t darwin_map[] = {
    {25, "macOS Tahoe"},           // macOS 26
    {24, "macOS Sequoia"},         // macOS 15
    {23, "macOS Sonoma"},          // macOS 14
    {22, "macOS Ventura"},         // macOS 13
    {21, "macOS Monterey"},        // macOS 12
    {20, "macOS Big Sur"},         // macOS 11
    {19, "macOS Catalina"},        // 10.15
    {18, "macOS Mojave"},          // 10.14
    {17, "macOS High Sierra"},     // 10.13
    {16, "macOS Sierra"},          // 10.12
    {15, "OS X El Capitan"},       // 10.11
    {14, "OS X Yosemite"},         // 10.10
    {13, "OS X Mavericks"},        // 10.9
    {12, "OS X Mountain Lion"},    // 10.8
    {11, "Mac OS X Lion"},         // 10.7
    {10, "Mac OS X Snow Leopard"}, // 10.6
    {9,  "Mac OS X Leopard"},      // 10.5
    {8,  "Mac OS X Tiger"},        // 10.4
    {7,  "Mac OS X Panther"},      // 10.3
    {6,  "Mac OS X Jaguar"},       // 10.2
    {5,  "Mac OS X Puma"},         // 10.1
    {0,  NULL}
};

/**
 * Pointer to the process environment variables array.
 * This is a POSIX-standard global variable used to pass the current
 * environment strings to child processes (e.g., via posix_spawn).
 */
extern char **environ;

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
 * Internal helper to identify the OS distribution and version.
 * First attempts to use sysctl (kern.osproductversion). 
 * If unavailable, falls back to spawning 'sw_vers'.
 *
 * @param out_buf  Destination buffer for the distro string.
 * @param buf_size Maximum size of the destination buffer.
 */
static void sys_get_distro(char *out_buf, const size_t buf_size);

/**
 * Executes a shell command and captures its standard output.
 * Uses posix_spawn for performance and safety on macOS.
 *
 * @param command  The shell command to execute.
 * @param out_buf  Buffer to store the command output.
 * @param buf_size Size of the output buffer.
 * @return 0 on success, non-zero on error.
 */
static int sys_exec_capture(const char *command, char *out_buf, 
                            const size_t buf_size);

/**
 * Maps the current Darwin release version to a macOS marketing name.
 * 
 * @return Pointer to a static string containing the OS name.
 */
static const char* sys_get_os_name(void);

/**
 * Formats the raw system uptime into a human-readable duration string.
 * Calculates duration by subtracting kern.boottime from current time.
 *
 * @param out_buf  Destination buffer for the formatted string.
 * @param buf_size Maximum size of the destination buffer.
 */
static void sys_format_uptime(char *out_buf, const size_t buf_size);

/**
 * Retrieves the total count of active processes in the system.
 * Uses a KERN_PROC_ALL sysctl snapshot.
 *
 * @param out_buf  Destination buffer for the count string.
 * @param buf_size Maximum size of the destination buffer.
 */
static void sys_get_procs_count(char *out_buf, const size_t buf_size);

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
}

void system_print_header(void) {
    char username[MAXLOGNAME] = {0};

    sys_get_identity(username, MAXLOGNAME);
    ui_render_header(username, sys_info.nodename);
}

void system_print_info(void) {
    char os_buf[LINE_BUFFER] = {0};
    sys_get_distro(os_buf, LINE_BUFFER);
    
    char device_model[LINE_BUFFER] = {0};
    size_t device_len = LINE_BUFFER;

    if (sysctlbyname("hw.model", device_model, &device_len, NULL, 0) != 0) {
        V_PRINTF("Error: get hw.model failed\n");
        snprintf(device_model, LINE_BUFFER, "unrecognized");
    }

    char uptime_buf[LINE_BUFFER] = {0};
    sys_format_uptime(uptime_buf, LINE_BUFFER);

    char procs_buf[SMALL_BUFFER] = {0};
    sys_get_procs_count(procs_buf, SMALL_BUFFER);

    ui_print_info("OS", os_buf);
    ui_print_info("Device", device_model);
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
        return;
    }

    snprintf(out_buf, buf_size, "%s", pwd->pw_name);
}

static void sys_get_distro(char *out_buf, const size_t buf_size) {
    char product_version[SMALL_BUFFER] = {0};
    size_t size = sizeof(product_version);

    snprintf(out_buf, buf_size, "%s ", sys_get_os_name());
    size_t current_len = strlen(out_buf);

    /* Try modern sysctl first (macOS 10.13.4+) */
    if (sysctlbyname("kern.osproductversion", product_version, &size, NULL, 0) == 0) {
        snprintf(out_buf + current_len, buf_size - current_len, "%s (%s)", 
                 product_version, sys_info.machine);
        return;
    }

    /* Fallback to sw_vers for older systems */
    if (sys_exec_capture("sw_vers -productVersion", product_version, sizeof(product_version)) == 0) {
        product_version[strcspn(product_version, "\r\n")] = 0;
    } else {
        strncpy(product_version, "unknown", sizeof(product_version) - 1);
    }

    snprintf(out_buf + current_len, buf_size - current_len, "%s (%s)", 
             product_version, sys_info.machine);
} 

static int sys_exec_capture(const char *command, char *out_buf, const size_t buf_size) {
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
        ssize_t bytes_read = read(pipefd[0], out_buf, buf_size - 1);
        if (bytes_read >= 0) out_buf[bytes_read] = '\0';
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
    } else {
        close(pipefd[0]);
        close(pipefd[1]);
    }

    posix_spawn_file_actions_destroy(&actions);
    return status;
}

static const char* sys_get_os_name(void) {
    int major = atoi(sys_info.release);

    for (int i = 0; darwin_map[i].name != NULL; i++) {
        if (darwin_map[i].major == major) {
            return darwin_map[i].name;
        }
    }

    return "macOS";
}

static void sys_format_uptime(char *out_buf, const size_t buf_size) {
    struct timeval boot_time = {0};
    size_t size = sizeof(boot_time);

    if (sysctlbyname("kern.boottime", &boot_time, &size, NULL, 0) != 0) {
        V_PRINTF("Error: get kern.boottime failed\n");
        snprintf(out_buf, buf_size, "--:--:--");
        return;
    }

    time_t now = time(NULL);
    time_t uptime = now - boot_time.tv_sec;
    if (uptime < 0) uptime = 0;

    time_t days = uptime / 86400;
    uptime %= 86400;
    time_t hours = uptime / 3600;
    uptime %= 3600;
    time_t minutes = uptime / 60;
    time_t seconds = uptime % 60;

    if (days > 0) {
        snprintf(out_buf, buf_size, "%ld days, %02ld:%02ld:%02ld", 
                 (long)days, (long)hours, (long)minutes, (long)seconds);
    } else {
        snprintf(out_buf, buf_size, "%02ld:%02ld:%02ld", 
                 (long)hours, (long)minutes, (long)seconds);
    }
}

static void sys_get_procs_count(char *out_buf, const size_t buf_size) {
    int mib[3] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL};
    size_t len;

    /* Initialize with fallback */
    snprintf(out_buf, buf_size, "-");

    /* First call to get the required buffer size */
    if (sysctl(mib, 3, NULL, &len, NULL, 0) < 0) {
        V_PRINTF("Error: failed to get process list size\n");
        return;
    }

    struct kinfo_proc *procs = malloc(len);
    if (!procs) {
        V_PRINTF("Error: malloc for kinfo_proc failed (%s)\n", strerror(errno));
        return;
    }

    /* Second call to fetch the actual process list */
    if (sysctl(mib, 3, procs, &len, NULL, 0) < 0) {
        V_PRINTF("Error: failed to fetch process list\n");
        free(procs);
        return;
    }

    size_t count = len / sizeof(struct kinfo_proc);
    snprintf(out_buf, buf_size, "%zu", count);

    free(procs);
}