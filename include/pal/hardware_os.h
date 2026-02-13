/* SPDX-License-Identifier: MIT */

#ifndef HARDWARE_OS_H
#define HARDWARE_OS_H

#include <stddef.h>
#include <stdint.h>

#include "defs.h"

#define KHZ_PER_GHZ   1e6
#define HZ_PER_GHZ    1e9

#define MAX_GPUS      16
#define GPU_BUFFER    (256 * MAX_GPUS)

#define BYTES_PER_KIB 1024
#define BYTES_PER_MIB (1024.0 * 1024.0)
#define BYTES_PER_GIB (1024.0 * 1024.0 * 1024.0)

typedef struct {
    char model[MEDIUM_BUFFER];
    char cores[MINI_BUFFER];
    char frequency[MINI_BUFFER];
} cpu_info_t;

typedef struct {
    uint64_t ram_size;
    uint64_t ram_used;
    uint64_t swap_size;
    uint64_t swap_used;
} mem_info_t;

typedef enum {
    MEM_RAM  = 1 << 0,
    MEM_SWAP = 1 << 1,
} mem_flags_t;

void hw_get_cpu_info(char *out_buf, const size_t buf_size);
void hw_get_cpu_model(cpu_info_t *node);
void hw_get_cpu_cores(cpu_info_t *node);
void hw_get_cpu_freq(cpu_info_t *node);

void hw_get_gpu_info(void);

void hw_get_mem_info(void);
void hw_print_mem_info(const mem_flags_t flags, const mem_info_t *node);

void hw_get_drives_info(void);

void hw_get_bat_info(void);

#endif /* HARDWARE_OS_H */