/* SPDX-License-Identifier: MIT */

#include "hardware.h"
#include "defs.h"
#include "sys_utils.h"
#include "ui.h"

#include "pal/hardware_os.h"

void hardware_print_info(void) {
    hw_get_cpu_info();
    hw_get_gpu_info();
    hw_get_mem_info();
    hw_get_drives_info();
    hw_get_bat_info();
}
