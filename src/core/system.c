/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <string.h>

#include "system.h"
#include "internal/system_os.h"
#include "ui.h"

sys_internal_data_t sys_data;

void system_print_header(void) {
    char username[SYS_USER_MAX] = {0};

    sys_get_identity(username, SYS_USER_MAX);
    ui_render_header(username, sys_data.nodename);
}

void system_print_info(void) {
    char os_buf[LINE_BUFFER] = {0};
    sys_get_distro(os_buf, LINE_BUFFER);
    
    char device_buf[LINE_BUFFER] = {0};
    sys_get_model_name(device_buf, LINE_BUFFER);

    char uptime_buf[LINE_BUFFER] = {0};
    sys_format_uptime(uptime_buf, LINE_BUFFER);

    char procs_buf[SMALL_BUFFER] = {0};
    snprintf(procs_buf, SMALL_BUFFER, "%hu", sys_data.procs);

    ui_print_info("OS", os_buf);
    if (strlen(device_buf) > 0) ui_print_info("Device", device_buf);
    ui_print_info("Kernel", sys_data.release);
    ui_print_info("Uptime", uptime_buf);
    ui_print_info("Processes", procs_buf);
}
