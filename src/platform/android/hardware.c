/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <inttypes.h>

#include <unistd.h>

#include <sys/system_properties.h>

#include "defs.h"
#include "sys_utils.h"
#include "ui.h"

#include "pal/hardware_os.h"

void hw_get_cpu_model(cpu_info_t *node) {
    if (__system_property_get("ro.soc.model", node->model) <= 0) {
        if (__system_property_get("ro.board.platform", node->model) <= 0) {
            V_PRINTF("[WARNING] Could not read cpu name, using 'Unknown'\n");
            snprintf(node->model, sizeof(node->model), "Unknown");
        }
    }
}

void hw_get_cpu_cores(cpu_info_t *node) {
    long cores = sysconf(_SC_NPROCESSORS_CONF);
    if (cores == -1) {
        V_PRINTF("[WARNING] Could not get cpu cores number: %s\n", strerror(errno));
        cores = 0;
    }

    snprintf(node->cores, sizeof(node->cores), " (%ld)", cores);
}

void hw_get_cpu_freq(cpu_info_t *node) {
    uint32_t freq_khz = 0;
    util_read_uint32("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", &freq_khz);

    double freq_ghz = (double)freq_khz / KHZ_PER_GHZ;
    snprintf(node->frequency, sizeof(node->frequency), " @ %.03f GHz", freq_ghz);
}

void hw_get_gpu_info(void) {
    /* TO:DO: Become a ninja Shaolin and learn the secret of obtaining a GPU model */

    return;
}

/*
 * NOTE: Battery parsing is skipped on non-rooted Android systems.
 * 1. Direct access to /sys/class/power_supply/ is blocked by Google since
 * Android 10+ to prevent battery-based fingerprinting (privacy protection).
 * 2. 'termux-api' bridge is too slow (~300-500ms overhead) for a utility
 *    aiming for sub-50ms execution time.
 * 3. 'dumpsys' requires android.permission.DUMP (not available by default).
 */
void hw_get_bat_info(void) {
    if (!util_is_file_exist("/sys/class/power_supply/battery/present"))
        return;

    uint8_t capacity = 0;
    char status[MINI_BUFFER] = {0};
    char health[MINI_BUFFER] = {0};

    util_read_uint8("/sys/class/power_supply/battery/capacity", &capacity);
    util_read_line("/sys/class/power_supply/battery/status", status, sizeof(status));
    util_read_line("/sys/class/power_supply/battery/health", health, sizeof(health));

    char information[LINE_BUFFER] = {0};
    snprintf(information, sizeof(information), "%" PRIu8 "(%s, Health: %s)", capacity, status,
             health);
    ui_print_info("Battery", information);
}
