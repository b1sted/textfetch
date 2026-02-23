/* SPDX-License-Identifier: MIT */

#ifndef HARDWARE_OS_H
#define HARDWARE_OS_H

#include <stddef.h>
#include <stdint.h>

#include "defs.h"

/* Multipliers for standardizing CPU clock speeds to Gigahertz. */
#define KHZ_PER_GHZ   1e6
#define HZ_PER_GHZ    1e9

/* Maximum number of distinct GPUs to query and store. */
#define MAX_GPUS      16

/* Maximum capacity for the formatted GPU output string buffer. */
#define GPU_BUFFER    (LINE_BUFFER * MAX_GPUS)

/* Mathematical constants for unit formatting. */
#define BYTES_PER_KIB 1024
#define BYTES_PER_MIB (1024.0 * 1024.0)
#define BYTES_PER_GIB (1024.0 * 1024.0 * 1024.0)

#if defined(__ANDROID__) || defined(__APPLE__)
/* Parsed CPU layout data (packages, model, topology, and speed). */
typedef struct cpu_info {
    uint16_t packages;
    char model[MEDIUM_BUFFER];
    char cores[MINI_BUFFER];
    char frequency[MINI_BUFFER];
} cpu_info_t;
#endif

/* Current memory utilization (total / used capacities for RAM and Swap). */
typedef struct {
    uint64_t ram_size;
    uint64_t ram_used;
    uint64_t swap_size;
    uint64_t swap_used;
} mem_info_t;

/* Bitmask flags indicating which memory types were successfully queried. */
typedef enum {
    MEM_RAM  = 1 << 0,
    MEM_SWAP = 1 << 1,
} mem_flags_t;

/* Gathers and prints CPU information (model, cores, frequency). */
void hw_get_cpu_info(void);

/* Gathers and prints GPU information (vendor, device, grouped). */
void hw_get_gpu_info(void);

/* Gathers memory information (RAM and Swap). */
void hw_get_mem_info(void);

/**
 * Formats and prints memory information.
 *
 * @param flags Specifies whether to print RAM, Swap, or both.
 * @param node Pointer to the populated memory info structure.
 */
void hw_print_mem_info(const mem_flags_t flags, const mem_info_t *node);

/* Gathers and prints information about attached drives/filesystems. */
void hw_get_drives_info(void);

/* Gathers and prints battery status, capacity, and model information. */
void hw_get_bat_info(void);

#endif /* HARDWARE_OS_H */