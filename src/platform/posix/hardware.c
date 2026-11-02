/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "sys_utils.h"
#include "ui.h"

#include "pal/hardware_os.h"

void hw_print_mem_info(const mem_flags_t flags, const mem_info_t *info)
{
    if (!info || (flags & (MEM_RAM | MEM_SWAP)) == 0) {
        V_PRINTF("[ERROR] %s: invalid arguments\n", __func__);
        return;
    }

    if (flags & MEM_RAM) {
        char ram_info[SMALL_BUFFER] = {0};

        util_format_size((double)info->ram_size, (double)info->ram_used, ram_info, sizeof(ram_info),
                         UNIT_B);
        ui_print_info("RAM", ram_info);
    }

    if (flags & MEM_SWAP) {
        char swap_info[SMALL_BUFFER] = {0};

        util_format_size((double)info->swap_size, (double)info->swap_used, swap_info,
                         sizeof(swap_info), UNIT_B);
        ui_print_info("Swap", swap_info);
    }
}