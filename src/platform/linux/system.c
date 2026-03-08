/* SPDX-License-Identifier: MIT */

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <ctype.h>
#include <errno.h>

#include "bitset.h"
#include "defs.h"
#include "dictionary.h"
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

/* Vendor names mapped to their preferred short acronyms. */
#define LONGEST_VENDOR_NAMES {                       \
    { "ASUSTeK COMPUTER INC.", "ASUS" },             \
    { "Micro-Star International Co., Ltd.", "MSI" }, \
    { "Hewlett-Packard", "HP" }                      \
}

/* Corporate suffixes and garbage words to be stripped from vendor names. */
#define GARBAGE_IN_VENDOR_NAME {           \
    "Technology",                          \
    "Corporation",                         \
    "Computer",                            \
    "Foundation",                          \
    "Electronics",                         \
    "Inc.",                                \
    "Co., Ltd.",                           \
    "Ltd.",                                \
    NULL                                   \
}

/* Specific vendor acronyms and brands that must remain fully capitalized. */
#define ALLCAPS_VENDOR_NAMES {             \
    "ASUS",                                \
    "AYANEO",                              \
    "AYN",                                 \
    "CHUWI",                               \
    "GPD",                                 \
    "HP",                                  \
    "LG",                                  \
    "MSI",                                 \
    "ONEXPLAYER",                          \
    "TECHO",                               \
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

/**
 * Checks if the DMI product family string is a placeholder or meaningless value.
 *
 * @param family_value The null-terminated product family string.
 * @return true if the value is considered garbage and should be skipped.
 */
static bool hw_is_family_garbage(char *family_value);

/**
 * Sanitizes and formats the raw vendor name (capitalization, acronyms, suffix stripping).
 *
 * @param vendor_name A pointer to the vendor string to be sanitized.
 * @param vendor_size The maximum capacity of the vendor_name buffer.
 */
static void hw_sanitize_vendor_name(char *vendor_name, const size_t vendor_size);

/**
 * Removes the vendor name from the beginning of the device model string if it is duplicated.
 *
 * @param vendor_name The sanitized vendor name string.
 * @param device_name A pointer to the device string from which the vendor name will be stripped.
 */
static void hw_remove_vendor_from_model(const char *vendor_name, char *device_name);
#endif

void sys_get_distro(char *out_buf, const size_t buf_size) {
    const char *os_release[] = OS_RELEASE_PATH;

    FILE *fp = NULL;
    for (uint8_t i = 0; os_release[i] != NULL; i++) {
        fp = fopen(os_release[i], "rb");
        if (fp) break;
    }

    if (!fp) {
        V_PRINTF("[ERROR] Failed to open os-release file: %s\n", strerror(errno));
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

    bool is_family_value_garbage = hw_is_family_garbage(family_buf);

    hw_sanitize_vendor_name(vendor_buf, sizeof(vendor_buf));
    hw_remove_vendor_from_model(vendor_buf, name_buf);

    if (is_family_value_garbage) {
        snprintf(out_buf, buf_size, "%s %s", vendor_buf, name_buf);
    } else {
        snprintf(out_buf, buf_size, "%s %s %s", vendor_buf,
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

static bool hw_is_family_garbage(char *family_value) {
    const char *ignore_product_family[] = IGNORE_SYS_PRODUCT_FAMILY;

    if (!util_string_in_array(family_value, ignore_product_family)) return false;

    return true;
}

static void hw_sanitize_vendor_name(char *vendor_name, const size_t vendor_size) {
    if (!vendor_name || vendor_size == 0) return;

    const char *vendor_pairs[][2] = LONGEST_VENDOR_NAMES;
    uint8_t vendor_pairs_count = sizeof(vendor_pairs) / sizeof(vendor_pairs[0]);

    const char *short_vendor_name = util_string_lookup(vendor_name, vendor_pairs,
                                                       vendor_pairs_count);

    if (short_vendor_name != NULL) {
        snprintf(vendor_name, vendor_size, "%s", short_vendor_name);
    }
        
    const char *vendor_garbage[] = GARBAGE_IN_VENDOR_NAME;
    char *garbage_pos = util_string_in_array(vendor_name, vendor_garbage);
    if (garbage_pos) {
        if (garbage_pos > vendor_name) garbage_pos--;
        *garbage_pos = '\0';
    }

    const char *allcaps_vendor[] = ALLCAPS_VENDOR_NAMES;

    bool is_allcaps = util_string_in_array(vendor_name, allcaps_vendor);

    for (uint8_t i = 1; i < strlen(vendor_name); i++) {
        unsigned char ch = (unsigned char)vendor_name[i];
        if (ch > 127) break;

        if (is_allcaps || !isupper(ch)) break;

        vendor_name[i] = (char)tolower(ch);
    }
}

static void hw_remove_vendor_from_model(const char *vendor_name, char *device_name) {
    if (!vendor_name || !device_name) return;

    char *dup_pos = strstr(device_name, vendor_name);
    if (dup_pos) {
        char *dup_end = dup_pos + strlen(vendor_name);
        if (*dup_end == ' ') dup_end++;

        memmove(dup_pos, dup_end, strlen(dup_end) + 1);
    }
}
#endif
