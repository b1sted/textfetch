/* SPDX-License-Identifier: MIT */

#include "system.h"

#include <string.h>

#include "../support/test_framework.h"
#include "../support/test_helpers.h"
#include "../support/test_stubs.h"
#include "config.h"
#include "hardware.h"
#include "pal/system_os.h"

static char *capture_void_call(void (*callback)(void))
{
    test_capture_t capture = {0};
    if (!test_capture_begin(&capture)) {
        return NULL;
    }

    callback();

    return test_capture_end(&capture);
}

static void test_hardware_order(void)
{
    stubs_reset();
    hw_print_info();
    TEST_STR_EQ(stub_hardware_calls, "cpu,gpu,mem,drives,battery");
}

static void test_print_header(void)
{
    cfg_set_color(false);
    stubs_reset();

    snprintf(sys_data.nodename, sizeof(sys_data.nodename), "%s", "test-host");
    snprintf(stub_identity, sizeof(stub_identity), "%s", "test-user");

    char *output = capture_void_call(sys_print_header);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "test-user@test-host\n-------------------\n");
    free(output);
}

static void test_print_info(void)
{
    cfg_set_color(false);
    stubs_reset();

    snprintf(sys_data.release, sizeof(sys_data.release), "%s", "6.1.0-test");
    sys_data.procs = 123;
    sys_data.threads = 0;

    char *output = capture_void_call(sys_print_info);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "OS: TestOS x86_64\n"
                        "Device: Test Device\n"
                        "Kernel: 6.1.0-test\n"
                        "Uptime: 1 days, 02:03:04\n"
                        "Processes: 123\n");
    free(output);
}

static void test_print_info_without_device(void)
{
    cfg_set_color(false);
    stubs_reset();

    snprintf(stub_model, sizeof(stub_model), "%s", "");
    snprintf(sys_data.release, sizeof(sys_data.release), "%s", "6.1.0-test");
    sys_data.procs = 1;
    sys_data.threads = 0;

    char *output = capture_void_call(sys_print_info);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "OS: TestOS x86_64\n"
                        "Kernel: 6.1.0-test\n"
                        "Uptime: 1 days, 02:03:04\n"
                        "Processes: 1\n");
    TEST_STR_NOT_CONTAINS(output, "Device:");
    free(output);
}

static void test_print_info_zero_processes(void)
{
    cfg_set_color(false);
    stubs_reset();

    snprintf(sys_data.release, sizeof(sys_data.release), "%s", "6.1.0-test");
    sys_data.procs = 0;
    sys_data.threads = 0;

    char *output = capture_void_call(sys_print_info);
    TEST_CHECK(output != NULL);
    TEST_STR_CONTAINS(output, "Processes: 0\n");
    free(output);
}

static void test_print_info_with_threads(void)
{
    cfg_set_color(false);
    stubs_reset();

    snprintf(sys_data.release, sizeof(sys_data.release), "%s", "6.1.0-test");
    sys_data.procs = 123;
    sys_data.threads = 456;

    char *output = capture_void_call(sys_print_info);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "OS: TestOS x86_64\n"
                        "Device: Test Device\n"
                        "Kernel: 6.1.0-test\n"
                        "Uptime: 1 days, 02:03:04\n"
                        "Processes: 123 (456 threads)\n");
    free(output);
}

int main(void)
{
    test_hardware_order();
    test_print_header();
    test_print_info();
    test_print_info_without_device();
    test_print_info_zero_processes();
    test_print_info_with_threads();

    return test_finish("system");
}
