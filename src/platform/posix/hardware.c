/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "sys_utils.h"
#include "ui.h"

#include "pal/hardware_os.h"

void hw_get_cpu_info(char *out_buf, const size_t buf_size) {
    cpu_info_t node;
    memset(&node, 0, sizeof(cpu_info_t));

    hw_get_cpu_model(&node);
    hw_get_cpu_cores(&node);
    hw_get_cpu_freq(&node);

    snprintf(out_buf, buf_size, "%s%s%s", node.model, node.cores, node.frequency);
}

void hw_print_mem_info(const mem_flags_t flags, const mem_info_t *node) {
    if (flags & MEM_RAM) {
        char ram_info[SMALL_BUFFER] = {0};

        util_format_size(node->ram_size, node->ram_used, ram_info, sizeof(ram_info), 
                         UNIT_B);
        ui_print_info("RAM", ram_info);
    }

    if (flags & MEM_SWAP) {
        char swap_info[SMALL_BUFFER] = {0};

        util_format_size(node->swap_size, node->swap_used, swap_info, sizeof(swap_info), 
                         UNIT_B);
        ui_print_info("Swap", swap_info);
    }
}