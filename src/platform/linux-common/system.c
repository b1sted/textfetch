/* SPDX-License-Identifier: MIT */

#include "pal/system_os.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <dirent.h>
#include <errno.h>

#include <sys/sysinfo.h>
#include <sys/utsname.h>

#include "defs.h"
#include "sys_utils.h"
#include "system.h"

/* Virtual filesystem path exposing one directory per process. */
#define PROC_PATH "/proc/"

/* Snapshot of the system load averages and task counts. */
#define LOADAVG_PATH "/proc/loadavg"

/**
 * Counts the processes visible through /proc.
 *
 * @param out_count Destination for the number of process directories.
 * @return true when the directory was read without errors.
 */
static bool sys_count_processes(uint32_t *out_count)
{
    if (out_count == NULL) {
        return false;
    }

    *out_count = 0;

    DIR *proc_dir = opendir(PROC_PATH);
    if (proc_dir == NULL) {
        V_PRINTF("[ERROR] opendir %s failed: %s\n", PROC_PATH, strerror(errno));
        return false;
    }

    uint32_t count = 0;
    errno = 0;
    struct dirent *entry = NULL;
    while ((entry = readdir(proc_dir)) != NULL) {
        if (util_is_numeric_string(entry->d_name)) {
            count++;
        }
    }

    bool ok = (errno == 0);
    if (!ok) {
        V_PRINTF("[ERROR] readdir %s failed: %s\n", PROC_PATH, strerror(errno));
    }

    if (closedir(proc_dir) != 0) {
        V_PRINTF("[ERROR] closedir %s failed: %s\n", PROC_PATH, strerror(errno));
        ok = false;
    }

    *out_count = count;
    return ok;
}

/**
 * Reads the total number of threads in the system.
 *
 * The fourth field of /proc/loadavg is "running/total", where "total" is the
 * kernel task counter (one task per thread, process leaders included). Unlike
 * the 16-bit sysinfo.procs field, this counter does not overflow on large
 * systems.
 *
 * @param out_count Destination for the total thread count.
 * @return true when the value was read and parsed successfully.
 */
static bool sys_read_total_threads(uint32_t *out_count)
{
    if (out_count == NULL) {
        return false;
    }

    *out_count = 0;

    FILE *file = fopen(LOADAVG_PATH, "r");
    if (file == NULL) {
        V_PRINTF("[ERROR] fopen %s failed: %s\n", LOADAVG_PATH, strerror(errno));
        return false;
    }

    char line[SMALL_BUFFER] = {0};
    bool read_ok = (fgets(line, sizeof(line), file) != NULL);

    if (fclose(file) != 0) {
        V_PRINTF("[ERROR] fclose %s failed: %s\n", LOADAVG_PATH, strerror(errno));
        return false;
    }

    if (!read_ok) {
        V_PRINTF("[ERROR] reading %s failed\n", LOADAVG_PATH);
        return false;
    }

    /* The only slash separates the runnable and total task counts. */
    const char *slash = strchr(line, '/');
    if (slash == NULL) {
        V_PRINTF("[ERROR] parsing %s failed\n", LOADAVG_PATH);
        return false;
    }

    errno = 0;
    char *end = NULL;
    uintmax_t total = strtoumax(slash + 1, &end, 10);
    if (errno != 0 || end == slash + 1 || (*end != '\0' && !isspace((unsigned char)*end)) ||
        total > UINT32_MAX) {
        V_PRINTF("[ERROR] parsing %s failed\n", LOADAVG_PATH);
        return false;
    }

    *out_count = (uint32_t)total;
    return true;
}

/**
 * Populates the process and thread counters.
 *
 * A process is a top-level /proc entry, while threads are all kernel tasks and
 * therefore include one thread per process. If the counters are unavailable
 * they stay at zero and the core omits the thread count from the output.
 */
static void sys_get_task_stats(void)
{
    sys_data.procs = 0;
    sys_data.threads = 0;

    if (!sys_count_processes(&sys_data.procs)) {
        return;
    }

    uint32_t total_threads = 0;
    if (!sys_read_total_threads(&total_threads)) {
        return;
    }

    /* Every process has at least one thread; a smaller total means that the
     * process list and the task counter were sampled at different times. */
    if (total_threads >= sys_data.procs) {
        sys_data.threads = total_threads;
    } else {
        V_PRINTF("[ERROR] inconsistent task counters: %" PRIu32 " processes, %" PRIu32 " threads\n",
                 sys_data.procs, total_threads);
    }
}

void sys_init(void)
{
    const char *fallback = "unknown";
    struct utsname uts = {0};
    struct sysinfo si = {0};

    bool uname_ok = (uname(&uts) == 0);
    if (!uname_ok) {
        V_PRINTF("[ERROR] uname failed: %s\n", strerror(errno));
    }

    snprintf(sys_data.sysname, sizeof(sys_data.sysname), "%s", uname_ok ? uts.sysname : fallback);
    snprintf(sys_data.nodename, sizeof(sys_data.nodename), "%s",
             uname_ok ? uts.nodename : fallback);
    snprintf(sys_data.release, sizeof(sys_data.release), "%s", uname_ok ? uts.release : fallback);
    snprintf(sys_data.machine, sizeof(sys_data.machine), "%s", uname_ok ? uts.machine : fallback);

    bool sysinfo_ok = (sysinfo(&si) == 0);
    if (!sysinfo_ok) {
        V_PRINTF("[ERROR] sysinfo failed: %s\n", strerror(errno));
    }

    sys_data.uptime = sysinfo_ok ? (uint64_t)si.uptime : 0;

    sys_get_task_stats();
}
