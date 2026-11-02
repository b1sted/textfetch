/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>

#include <sys/sysinfo.h>
#include <sys/utsname.h>

#include "defs.h"
#include "sys_utils.h"
#include "system.h"

#include "pal/system_os.h"

void system_init(void)
{
    const char *fallback = "unknown";
    struct utsname uts;
    struct sysinfo si;

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

    /* Linux exposes the process count through a 16-bit sysinfo field. */
    sys_data.procs = sysinfo_ok ? (uint32_t)si.procs : 0;
}
