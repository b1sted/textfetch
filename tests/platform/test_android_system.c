/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pal/hardware_os.h"
#include "pal/system_os.h"
#include "../support/test_framework.h"

/* Include the translation units to exercise them against a mocked bionic API. */
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../../src/platform/android/system.c"

#if !defined(__ANDROID__) && !defined(__APPLE__)
/* android/hardware.c is only built for Android; mirror the CPU info layout so
 * the translation unit can also be exercised on the host. */
typedef struct cpu_info {
    uint16_t packages;
    char model[MEDIUM_BUFFER];
    char cores[MINI_BUFFER];
    char frequency[MINI_BUFFER];
} cpu_info_t;
#endif

// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../../src/platform/android/hardware.c"

#define MAX_TEST_PROPERTIES 8

typedef struct {
    const char *name;
    const char *value;
} test_property_t;

static test_property_t test_properties[MAX_TEST_PROPERTIES];
static char test_property_values[MAX_TEST_PROPERTIES][PROP_VALUE_MAX];
static size_t test_property_count = 0;

sys_internal_data_t sys_data;

static void test_properties_reset(void) {
    memset(test_properties, 0, sizeof(test_properties));
    memset(test_property_values, 0, sizeof(test_property_values));
    test_property_count = 0;
}

static void test_property_set(const char *name, const char *value) {
    if (test_property_count >= MAX_TEST_PROPERTIES) return;

    test_properties[test_property_count].name = name;
    snprintf(test_property_values[test_property_count],
             sizeof(test_property_values[test_property_count]), "%s", value);
    test_properties[test_property_count].value =
        test_property_values[test_property_count];
    test_property_count++;
}

int __system_property_get(const char *name, char *value) {
    if (!name || !value) return 0;

    for (size_t i = 0; i < test_property_count; i++) {
        if (strcmp(test_properties[i].name, name) != 0) continue;

        size_t length = strlen(test_properties[i].value);
        if (length >= PROP_VALUE_MAX) length = PROP_VALUE_MAX - 1;

        memcpy(value, test_properties[i].value, length);
        value[length] = '\0';
        return (int)length;
    }

    value[0] = '\0';
    return 0;
}

static bool test_battery_present = true;
static uint8_t test_battery_capacity = 85;
static const char *test_battery_status = NULL;
static const char *test_battery_health = NULL;

static char test_ui_label[MINI_BUFFER] = {0};
static char test_ui_information[LINE_BUFFER] = {0};

bool util_is_file_exist(const char *path) {
    return test_battery_present && path != NULL &&
           strcmp(path, "/sys/class/power_supply/battery/present") == 0;
}

bool util_read_uint8(const char *path, uint8_t *value) {
    if (!path || !value || !test_battery_present) return false;
    if (strcmp(path, "/sys/class/power_supply/battery/capacity") != 0) {
        return false;
    }

    *value = test_battery_capacity;
    return true;
}

bool util_read_line(const char *path, char *out_buf, const size_t buf_size) {
    if (!path || !out_buf || buf_size == 0) return false;

    out_buf[0] = '\0';

    const char *value = NULL;
    if (strcmp(path, "/sys/class/power_supply/battery/status") == 0) {
        value = test_battery_status;
    } else if (strcmp(path, "/sys/class/power_supply/battery/health") == 0) {
        value = test_battery_health;
    }

    if (!value || value[0] == '\0') return false;

    snprintf(out_buf, buf_size, "%s", value);
    return true;
}

bool util_read_uint32(const char *path, uint32_t *value) {
    (void)path;
    (void)value;
    return false;
}

bool util_is_numeric_string(const char *str) {
    if (!str || *str == '\0') return false;

    for (const char *cursor = str; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') return false;
    }

    return true;
}

void ui_print_info(const char *label, const char *information) {
    snprintf(test_ui_label, sizeof(test_ui_label), "%s", label ? label : "");
    snprintf(test_ui_information, sizeof(test_ui_information), "%s",
             information ? information : "");
}

static void check_model_name(const char *brand, const char *model,
                             const char *name, const char *expected) {
    char buffer[512] = {0};

    test_properties_reset();
    test_property_set("ro.product.brand", brand);
    test_property_set("ro.product.model", model);
    test_property_set("ro.product.name", name);

    sys_get_model_name(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, expected);
}

static void test_model_name(void) {
    check_model_name("Google", "Pixel 8", "shiba", "Google Pixel 8 (shiba)");
    check_model_name("Google", "Google Pixel 8", "shiba",
                     "Google Pixel 8 (shiba)");
    check_model_name("Google", "Google", "google", "Google");
    check_model_name("Google", "shiba", "shiba", "Google shiba");
}

static void test_model_name_fallbacks(void) {
    char buffer[512] = {0};

    test_properties_reset();
    sys_get_model_name(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "Unknown");
}

static void test_long_property_values(void) {
    char brand[PROP_VALUE_MAX];
    char model[PROP_VALUE_MAX];
    char name[PROP_VALUE_MAX];
    char expected[512] = {0};
    char buffer[512] = {0};

    memset(brand, 'B', sizeof(brand) - 1);
    brand[sizeof(brand) - 1] = '\0';
    memset(model, 'M', sizeof(model) - 1);
    model[sizeof(model) - 1] = '\0';
    memset(name, 'N', sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';

    test_properties_reset();
    test_property_set("ro.product.brand", brand);
    test_property_set("ro.product.model", model);
    test_property_set("ro.product.name", name);

    snprintf(expected, sizeof(expected), "%s %s (%s)", brand, model, name);
    sys_get_model_name(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, expected);
}

static void test_distro(void) {
    char buffer[512] = {0};

    snprintf(sys_data.machine, sizeof(sys_data.machine), "%s", "aarch64");

    test_properties_reset();
    test_property_set("ro.build.version.release", "14");
    sys_get_distro(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "Android 14 aarch64");

    test_properties_reset();
    sys_get_distro(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "Android Unknown aarch64");
}

static void test_system_guards(void) {
    char buffer[16] = "unchanged";

    sys_get_distro(NULL, 0);
    sys_get_distro(buffer, 0);
    TEST_STR_EQ(buffer, "unchanged");

    sys_get_model_name(NULL, 0);
    sys_get_model_name(buffer, 0);
    TEST_STR_EQ(buffer, "unchanged");
}

static void test_battery_fallbacks(void) {
    test_battery_present = true;
    test_battery_capacity = 85;

    test_battery_status = NULL;
    test_battery_health = NULL;
    hw_get_bat_info();
    TEST_STR_EQ(test_ui_label, "Battery");
    TEST_STR_EQ(test_ui_information, "85% (Unknown, Health: Unknown)");

    test_battery_status = "";
    test_battery_health = "";
    hw_get_bat_info();
    TEST_STR_EQ(test_ui_information, "85% (Unknown, Health: Unknown)");

    test_battery_status = "Discharging";
    test_battery_health = "";
    hw_get_bat_info();
    TEST_STR_EQ(test_ui_information, "85% (Discharging, Health: Unknown)");

    test_battery_status = "Charging";
    test_battery_health = "Good";
    hw_get_bat_info();
    TEST_STR_EQ(test_ui_information, "85% (Charging, Health: Good)");
}

static void test_long_distro_version(void) {
    char version[PROP_VALUE_MAX];
    char expected[512] = {0};
    char buffer[512] = {0};

    memset(version, 'V', sizeof(version) - 1);
    version[sizeof(version) - 1] = '\0';

    snprintf(sys_data.machine, sizeof(sys_data.machine), "%s", "aarch64");

    test_properties_reset();
    test_property_set("ro.build.version.release", version);

    snprintf(expected, sizeof(expected), "Android %s aarch64", version);
    sys_get_distro(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, expected);
}

int main(void) {
    test_model_name();
    test_model_name_fallbacks();
    test_long_property_values();
    test_distro();
    test_system_guards();
    test_battery_fallbacks();
    test_long_distro_version();

    return test_finish("android_system");
}
