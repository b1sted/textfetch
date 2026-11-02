/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pal/system_os.h"
#include "test_framework.h"

/* Include the translation unit to exercise it against a mocked bionic API. */
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../src/platform/android/system.c"

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
    test_long_distro_version();

    return test_finish("android_system");
}
