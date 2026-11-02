/* SPDX-License-Identifier: MIT */

#include "ui.h"

#include <string.h>

#include "../support/test_framework.h"
#include "../support/test_helpers.h"
#include "config.h"

static char *capture_void_call(void (*callback)(void))
{
    test_capture_t capture = {0};
    if (!test_capture_begin(&capture)) {
        return NULL;
    }

    callback();

    return test_capture_end(&capture);
}

static void call_print_info(void) { ui_print_info("OS", "Linux"); }

static void call_print_info_empty(void) { ui_print_info("Label", ""); }

static void call_render_header(void) { ui_render_header("user", "host"); }

static void call_render_header_empty(void) { ui_render_header("", ""); }

static void call_print_help(void) { ui_print_help("textfetch"); }

static void call_print_version(void) { ui_print_version("1.2.3"); }

static void test_print_info(void)
{
    cfg_set_color(false);

    char *output = capture_void_call(call_print_info);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "OS: Linux\n");
    free(output);

    output = capture_void_call(call_print_info_empty);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "Label: \n");
    free(output);
}

static void test_render_header(void)
{
    cfg_set_color(false);

    char *output = capture_void_call(call_render_header);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "user@host\n---------\n");
    free(output);

    output = capture_void_call(call_render_header_empty);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "@\n-\n");
    free(output);
}

static void test_help(void)
{
    cfg_set_color(false);

    char *output = capture_void_call(call_print_help);
    TEST_CHECK(output != NULL);

    TEST_STR_CONTAINS(output, "Usage: textfetch [OPTIONS]");
    TEST_STR_CONTAINS(output, "Output Formatting:");
    TEST_STR_CONTAINS(output, "-h, --human-readable");
    TEST_STR_CONTAINS(output, "-k, --kibibytes");
    TEST_STR_CONTAINS(output, "-m, --mebibytes");
    TEST_STR_CONTAINS(output, "-g, --gibibytes");
    TEST_STR_CONTAINS(output, "-v, --verbose");
    TEST_STR_CONTAINS(output, "--no-color");
    TEST_STR_CONTAINS(output, "--help");
    TEST_STR_CONTAINS(output, "-V, --version");

    free(output);
}

static void test_version(void)
{
    cfg_set_color(false);

    char *output = capture_void_call(call_print_version);
    TEST_CHECK(output != NULL);

    TEST_STR_CONTAINS(output, "textfetch v1.2.3");
    if (output) {
        TEST_CHECK(output[strlen(output) - 1] == '\n');
    }

    free(output);
}

static void test_color_output(void)
{
    cfg_set_color(true);

    char *output = capture_void_call(call_print_info);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "\x1b[1mOS\x1b[0m: Linux\n");
    free(output);

    output = capture_void_call(call_render_header);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "\x1b[1muser@host\x1b[0m\n---------\n");
    free(output);

    output = capture_void_call(call_print_help);
    TEST_CHECK(output != NULL);
    TEST_STR_CONTAINS(output, "\x1b[1mUsage:\x1b[0m");
    free(output);

    cfg_set_color(false);
    output = capture_void_call(call_print_info);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "OS: Linux\n");
    free(output);
}

int main(void)
{
    test_print_info();
    test_render_header();
    test_help();
    test_version();
    test_color_output();

    return test_finish("ui");
}
