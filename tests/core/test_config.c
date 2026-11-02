/* SPDX-License-Identifier: MIT */

#include "config.h"
#include "../support/test_framework.h"

static void test_defaults(void) {
    TEST_CHECK(!cfg_get_kib());
    TEST_CHECK(!cfg_get_mib());
    TEST_CHECK(!cfg_get_gib());
    TEST_CHECK(!cfg_get_verbose());
    TEST_CHECK(!cfg_get_color());
}

static void test_flags(void) {
    cfg_set_kib();
    TEST_CHECK(cfg_get_kib());
    TEST_CHECK(!cfg_get_mib());
    TEST_CHECK(!cfg_get_gib());
    TEST_CHECK(!cfg_get_verbose());

    cfg_set_mib();
    TEST_CHECK(cfg_get_kib());
    TEST_CHECK(cfg_get_mib());
    TEST_CHECK(!cfg_get_gib());

    cfg_set_gib();
    TEST_CHECK(cfg_get_kib());
    TEST_CHECK(cfg_get_mib());
    TEST_CHECK(cfg_get_gib());

    cfg_set_verbose();
    TEST_CHECK(cfg_get_verbose());

    cfg_set_kib();
    cfg_set_mib();
    cfg_set_gib();
    cfg_set_verbose();
    TEST_CHECK(cfg_get_kib());
    TEST_CHECK(cfg_get_mib());
    TEST_CHECK(cfg_get_gib());
    TEST_CHECK(cfg_get_verbose());
}

static void test_color(void) {
    TEST_CHECK(!cfg_get_color());

    cfg_set_color(true);
    TEST_CHECK(cfg_get_color());

    cfg_set_color(false);
    TEST_CHECK(!cfg_get_color());

    cfg_set_color(true);
    TEST_CHECK(cfg_get_color());
}

static void test_version_string(void) {
    TEST_STR_EQ(version_string, APP_VERSION);
    TEST_STR_EQ(version_string, "1.0");
}

int main(void) {
    test_defaults();
    test_flags();
    test_color();
    test_version_string();

    return test_finish("config");
}
