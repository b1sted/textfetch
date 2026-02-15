/* SPDX-License-Identifier: MIT */

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <errno.h>

#include "bitset.h"
#include "defs.h"
#include "sys_utils.h"

#include "pal/system_os.h"

#if !(defined(__arm__) || defined(__aarch64__) || defined(__riscv) || defined(__powerpc__))
static bool sys_is_portable(void);
#endif

void sys_get_distro(char *out_buf, const size_t buf_size) {
    FILE *fp = fopen("/etc/os-release", "r");
    if (!fp) {
        fp = fopen("/usr/lib/os-release", "r");

        if (!fp) {
            V_PRINTF("Error: open os-release file failed: %s\n", strerror(errno));
            snprintf(out_buf, buf_size, "%s %s", sys_data.sysname, sys_data.machine);
            return;
        }
    }

    char line[LINE_BUFFER];
    while (fgets(line, sizeof(line), fp)) {
        char *delim = strchr(line, '=');
        if (!delim) continue;

        *delim = 0;
        char *key = line;
        char *value = delim + 1;

        value[strcspn(value, "\n")] = 0;

        if (value[0] == '"') {
            value++;

            size_t len = strlen(value);

            if (len > 0 && value[len - 1] == '"') {
                value[len - 1] = 0;
            }
        }

        if (strcmp(key, "NAME") == 0) {
            snprintf(out_buf, buf_size, "%s", value);
        }

        if (strcmp(key, "PRETTY_NAME") == 0) {
            snprintf(out_buf, buf_size, "%s", value);
            break;
        }
    }

    fclose(fp);

    size_t len = strlen(out_buf);
    snprintf(out_buf + len, buf_size - len, " %s", sys_data.machine);
}

#if defined(__arm__) || defined(__aarch64__) || defined(__riscv) || defined(__powerpc__)
void sys_get_model_name(char *out_buf, const size_t buf_size) {
    if (!util_read_line("/proc/device-tree/model", out_buf, buf_size)) {
        if (!util_read_line("/sys/firmware/devicetree/base/model", out_buf, buf_size)) {
            V_PRINTF("[ERROR] Failed to obtain model name from Device Tree\n");
        }
    }
}
#else
void sys_get_model_name(char *out_buf, const size_t buf_size) {
    if (!sys_is_portable()) return;

    char vendor_buf[SMALL_BUFFER] = {0};
    char family_buf[MEDIUM_BUFFER] = {0};
    char name_buf[MEDIUM_BUFFER] = {0};

    util_read_line("/sys/class/dmi/id/sys_vendor", vendor_buf, sizeof(vendor_buf));
    util_read_line("/sys/class/dmi/id/product_family", family_buf, sizeof(family_buf));
    util_read_line("/sys/class/dmi/id/product_name", name_buf, sizeof(name_buf));

    const char *family_trash_values[] = {
        "00000000", "11111111", "All Series", "BDW", "CFL", "CHASSIS", "CNL",
        "Default String", "Family", "Generic", "HSW", "ICL", "Invalid", "KBL",
        "LBG", "None", "Not Specified", "SKL", "System Family", "TGL",
        "To Be Filled By O.E.M.", "Unknown", "Whiskey Lake", NULL
    };

    bool is_family_value_garbage = false;
    for (int i = 0; family_trash_values[i] != NULL; i++) {
        if (strcasecmp(family_buf, family_trash_values[i]) == 0) {
            is_family_value_garbage = true;
            break;
        }
    }

    const char *final_vendor = (strcasestr(name_buf, vendor_buf)) ? "" : vendor_buf;

    if (is_family_value_garbage) {
        snprintf(out_buf, buf_size, "%s %s", final_vendor, name_buf);
    } else {
        snprintf(out_buf, buf_size, "%s %s %s", final_vendor,
                 (strcasestr(name_buf, family_buf)) ? "" : family_buf, name_buf);
    }
}

static bool sys_is_portable(void) {
    uint8_t chassis_value = 0;
    if (!util_read_uint8("/sys/class/dmi/id/chassis_type", &chassis_value)) return false;

    const uint32_t dmi_portable_chassis_mask[SET_SIZE] = {
        [0] = BIT(8) | BIT(9) | BIT(10) | BIT(11) | BIT(14) | BIT(30) | BIT(31), 
        [1] = BIT(32)
    };

    if (set_contains(dmi_portable_chassis_mask, chassis_value)) return true;

    return false;
}
#endif
