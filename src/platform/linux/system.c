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

/* Standard paths for accessing OS release information. */
#define OS_RELEASE_PATH {                  \
    "/etc/os-release",                     \
    "/usr/lib/os-release",                 \
    NULL                                   \
}

/* Paths corresponding to the device tree model string on ARM platforms. */
#define DEVICE_MODEL_FILE {                \
    "/proc/device-tree/model",             \
    "/sys/firmware/devicetree/base/model", \
    NULL                                   \
}

/* Placeholder or garbage DMI product family strings to ignore in the final output. */
#define IGNORE_SYS_PRODUCT_FAMILY {        \
    "00000000",                            \
    "11111111",                            \
    "All Series",                          \
    "BDW",                                 \
    "CFL",                                 \
    "CHASSIS",                             \
    "CNL",                                 \
    "Default String",                      \
    "Family",                              \
    "Generic",                             \
    "HSW",                                 \
    "ICL",                                 \
    "Invalid",                             \
    "KBL",                                 \
    "LBG",                                 \
    "None",                                \
    "Not Specified",                       \
    "SKL",                                 \
    "System Family",                       \
    "TGL",                                 \
    "To Be Filled By O.E.M.",              \
    "Unknown",                             \
    "Whiskey Lake",                        \
    NULL                                   \
}

/* Sysfs DMI tables used to construct the full hardware marketing model name. */
#define SYS_VENDOR_PATH         "/sys/class/dmi/id/sys_vendor"
#define SYS_PRODUCT_FAMILY_PATH "/sys/class/dmi/id/product_family"
#define SYS_PRODUCT_NAME_PATH   "/sys/class/dmi/id/product_name"
#define PATH_DMI_CHASSIS        "/sys/class/dmi/id/chassis_type"

/* DMI chassis type bitmask parameters used to identifying portable devices (laptops). */
#define CHASSIS_MASK_ELEMENTS 64
#define CHASSIS_MASK_BLOCKS   (CHASSIS_MASK_ELEMENTS / BITS_PER_BLOCK)

#if !(defined(__arm__) || defined(__aarch64__) || defined(__riscv) || defined(__powerpc__))
/**
 * Determines if the system is a portable device (laptop) using DMI chassis type.
 *
 * @return true if the system chassis identifies as portable.
 */
static bool sys_is_portable(void);
#endif

void sys_get_distro(char *out_buf, const size_t buf_size) {
    const char *os_release[] = OS_RELEASE_PATH;

    FILE *fp = NULL;
    for (uint8_t i = 0; os_release[i] != NULL; i++) {
        fp = fopen(os_release[i], "rb");
        if (fp) break;
    }

    if (!fp) {
        V_PRINTF("[ERROR] Fail to open os-release file: %s\n", strerror(errno));
        snprintf(out_buf, buf_size, "%s %s", sys_data.sysname, sys_data.machine);
        return;
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
    const char *model_file[] = DEVICE_MODEL_FILE;

    for (uint8_t i = 0; model_file[i] != NULL; i++) {
        if (util_read_line(model_file[i], out_buf, buf_size)) return;
    }

    V_PRINTF("[ERROR] Failed to obtain model name from Device Tree\n");
}
#else
void sys_get_model_name(char *out_buf, const size_t buf_size) {
    if (!sys_is_portable()) return;

    char vendor_buf[SMALL_BUFFER] = {0};
    char family_buf[MEDIUM_BUFFER] = {0};
    char name_buf[MEDIUM_BUFFER] = {0};

    util_read_line(SYS_VENDOR_PATH, vendor_buf, sizeof(vendor_buf));
    util_read_line(SYS_PRODUCT_FAMILY_PATH, family_buf, sizeof(family_buf));
    util_read_line(SYS_PRODUCT_NAME_PATH, name_buf, sizeof(name_buf));

    const char *ignore_product_family[] = IGNORE_SYS_PRODUCT_FAMILY;

    bool is_family_value_garbage = false;
    for (int i = 0; ignore_product_family[i] != NULL; i++) {
        if (strcasecmp(family_buf, ignore_product_family[i]) == 0) {
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
    if (!util_read_uint8(PATH_DMI_CHASSIS, &chassis_value)) return false;

    uint32_t dmi_portable_chassis_bits[CHASSIS_MASK_BLOCKS] = {
        [0] = BIT(8) | BIT(9) | BIT(10) | BIT(11) | BIT(14) | BIT(30) | BIT(31),
        [1] = BIT(32)
    };

    bitset_t chassis_set = {.bits = dmi_portable_chassis_bits,
                            .capacity = CHASSIS_MASK_ELEMENTS};

    if (set_contains(&chassis_set, chassis_value)) return true;

    return false;
}
#endif
