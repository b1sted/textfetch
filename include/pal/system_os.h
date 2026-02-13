/* SPDX-License-Identifier: MIT */

#ifndef SYSTEM_OS_H
#define SYSTEM_OS_H

#include <stddef.h>
#include <stdint.h>

#define SYSINFO_BUFFER_SIZE 128
#ifndef PROP_NAME_MAX
#define PROP_NAME_MAX       128
#endif

typedef struct {
    /* System info (via uname) */
    char sysname[SYSINFO_BUFFER_SIZE];  /* OS name */
    char nodename[SYSINFO_BUFFER_SIZE]; /* Hostname */
    char release[SYSINFO_BUFFER_SIZE];  /* Kernel version */
    char machine[SYSINFO_BUFFER_SIZE];  /* Hardware arch */

    /* Dynamic stats (sysinfo on Linux, sysctl on macOS) */
    uint64_t uptime;                    /* Seconds since boot */
    uint32_t procs;                     /* Process count */
} sys_internal_data_t;

extern sys_internal_data_t sys_data;

void sys_get_identity(char *out_buf, const size_t buf_size);
void sys_get_model_name(char *out_buf, size_t buf_size);
void sys_get_distro(char *out_buf, const size_t buf_size);
void sys_format_uptime(char *out_buf, const size_t buf_size);

#endif /* SYSTEM_OS_H */