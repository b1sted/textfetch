/* SPDX-License-Identifier: MIT */

#include "hardware.h"
#include "defs.h"
#include "sys_utils.h"
#include "ui.h"

#include "pal/hardware_os.h"

void hardware_print_info(void) {
    char cpu_buf[LINE_BUFFER] = {0};
    hw_get_cpu_info(cpu_buf, LINE_BUFFER);
    ui_print_info("CPU", cpu_buf);

    hw_get_gpu_info();
    hw_get_mem_info();
    hw_get_drives_info();
    hw_get_bat_info();
}
