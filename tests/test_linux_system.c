/* SPDX-License-Identifier: MIT */

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "pal/system_os.h"
#include "system.h"
#include "test_framework.h"

/* The platform sources only need the shared data block. */
sys_internal_data_t sys_data;

/* Include the translation unit to exercise its static helpers directly. */
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../src/platform/linux/system.c"

static void test_system_init(void) {
    system_init();

    TEST_CHECK(strlen(sys_data.sysname) > 0);
    TEST_CHECK(strlen(sys_data.nodename) > 0);
    TEST_CHECK(strlen(sys_data.release) > 0);
    TEST_CHECK(strlen(sys_data.machine) > 0);
    TEST_CHECK(sys_data.uptime > 0);
    TEST_CHECK(sys_data.procs > 0);
}

static void test_get_distro(void) {
    char buffer[256] = {0};

    sys_get_distro(buffer, sizeof(buffer));
    TEST_CHECK(strlen(buffer) > 0);
    TEST_STR_CONTAINS(buffer, sys_data.machine);

    char small[4] = {0};
    sys_get_distro(small, sizeof(small));
    TEST_CHECK(strlen(small) < sizeof(small));
}

static void test_get_model_name(void) {
    char buffer[128] = {0};

    sys_get_model_name(buffer, sizeof(buffer));
    TEST_CHECK(strlen(buffer) < sizeof(buffer));

    TEST_CHECK(sys_is_portable() == true || sys_is_portable() == false);
}

static void test_family_garbage(void) {
    char empty[] = "";
    char generic[] = "Generic";
    char default_string[] = "Default String";
    char filled[] = "To Be Filled By O.E.M.";
    char not_specified[] = "Not Specified";
    char custom[] = "Custom Model";
    char family[] = "Custom Family";

    TEST_CHECK(!hw_is_family_garbage(empty));
    TEST_CHECK(hw_is_family_garbage(generic));
    TEST_CHECK(hw_is_family_garbage(default_string));
    TEST_CHECK(hw_is_family_garbage(filled));
    TEST_CHECK(hw_is_family_garbage(not_specified));
    TEST_CHECK(!hw_is_family_garbage(custom));
    TEST_CHECK(hw_is_family_garbage(family));
}

static void check_vendor(const char *input, const char *expected) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s", input);
    hw_sanitize_vendor_name(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, expected);
}

static void test_sanitize_vendor(void) {
    check_vendor("ASUSTeK COMPUTER INC.", "ASUS");
    check_vendor("Micro-Star International Co., Ltd.", "MSI");
    check_vendor("Hewlett-Packard", "HP");
    check_vendor("Dell Inc.", "Dell");
    check_vendor("LENOVO", "Lenovo");
    check_vendor("HP", "HP");
    check_vendor("Apple Inc.", "Apple");
    check_vendor("Foo Technology Bar", "Foo");
    check_vendor("Technology", "");
    check_vendor("Unknown", "Unknown");
    check_vendor("\xC3\x84pfel", "\xC3\x84pfel");

    hw_sanitize_vendor_name(NULL, 128);

    char buffer[64] = "Dell Inc.";
    hw_sanitize_vendor_name(buffer, 0);
    TEST_STR_EQ(buffer, "Dell Inc.");
}

static void check_remove_vendor(const char *vendor, const char *model,
                                const char *expected) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s", model);
    hw_remove_vendor_from_model(vendor, buffer);
    TEST_STR_EQ(buffer, expected);
}

static void test_remove_vendor_from_model(void) {
    check_remove_vendor("ASUS", "ASUS ROG Strix", "ROG Strix");
    check_remove_vendor("ASUS", "ROG ASUS Strix", "ROG Strix");
    check_remove_vendor("ASUS", "ASUSROG", "ROG");
    check_remove_vendor("ASUS", "ROG Strix", "ROG Strix");
    check_remove_vendor("ASUS", "ASUS", "");
    check_remove_vendor("", "ROG Strix", "ROG Strix");

    hw_remove_vendor_from_model(NULL, NULL);

    char model[] = "ASUS ROG";
    hw_remove_vendor_from_model(NULL, model);
    TEST_STR_EQ(model, "ASUS ROG");
    hw_remove_vendor_from_model("ASUS", NULL);
}

int main(void) {
    test_system_init();
    test_get_distro();
    test_get_model_name();
    test_family_garbage();
    test_sanitize_vendor();
    test_remove_vendor_from_model();

    return test_finish("linux_system");
}
