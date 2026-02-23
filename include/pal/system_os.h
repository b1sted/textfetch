/* SPDX-License-Identifier: MIT */

#ifndef SYSTEM_OS_H
#define SYSTEM_OS_H

#include <stddef.h>
#include <stdint.h>

/* Buffer length for caching OS property strings. */
#define SYSINFO_BUFFER_SIZE 128

/* Fallback limit for property name length. */
#ifndef PROP_NAME_MAX
#define PROP_NAME_MAX       128
#endif

/* Core system data (OS identity, kernel, uptime, and process count). */
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

/* Global system data instance accessible across platform modules. */
extern sys_internal_data_t sys_data;

/**
 * Retrieves the OS identity and version information.
 *
 * @param out_buf Buffer to store the formatted OS identity string.
 * @param buf_size Maximum size of the buffer.
 */
void sys_get_identity(char *out_buf, const size_t buf_size);

/**
 * Retrieves the hardware model name of the system.
 *
 * @param out_buf Buffer to store the formatted model name string.
 * @param buf_size Maximum size of the buffer.
 */
void sys_get_model_name(char *out_buf, size_t buf_size);

/**
 * Retrieves the detailed OS distribution name.
 *
 * @param out_buf Buffer to store the formatted distro string.
 * @param buf_size Maximum size of the buffer.
 */
void sys_get_distro(char *out_buf, const size_t buf_size);

/**
 * Formats the system uptime into a human-readable string.
 *
 * @param out_buf Buffer to store the formatted uptime.
 * @param buf_size Maximum size of the buffer.
 */
void sys_format_uptime(char *out_buf, const size_t buf_size);

#endif /* SYSTEM_OS_H */