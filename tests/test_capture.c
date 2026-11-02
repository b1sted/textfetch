/* SPDX-License-Identifier: MIT */

#include <string.h>

#include "capture.h"
#include "test_framework.h"

static void test_successful_capture(void) {
    char buffer[64] = {0};

    TEST_INT_EQ(capture_line("echo", "hello", buffer, sizeof(buffer)), 0);
    TEST_STR_EQ(buffer, "hello");

    memset(buffer, 0, sizeof(buffer));
    TEST_INT_EQ(capture_line("echo", "hello world", buffer, sizeof(buffer)), 0);
    TEST_STR_EQ(buffer, "hello world");

    memset(buffer, 0, sizeof(buffer));
    TEST_INT_EQ(capture_line("printf", "first\\nsecond\\n", buffer,
                             sizeof(buffer)), 0);
    TEST_STR_EQ(buffer, "first");

    memset(buffer, 0, sizeof(buffer));
    TEST_INT_EQ(capture_line("uname", NULL, buffer, sizeof(buffer)), 0);
    TEST_CHECK(strlen(buffer) > 0);
    TEST_STR_NOT_CONTAINS(buffer, "\n");
}

static void test_truncation(void) {
    char buffer[5] = {'X', 'X', 'X', 'X', 'X'};

    TEST_INT_EQ(capture_line("echo", "0123456789", buffer, sizeof(buffer)), 0);
    TEST_STR_EQ(buffer, "0123");
}

static void test_empty_output(void) {
    char buffer[32] = {'X'};

    TEST_INT_EQ(capture_line("echo", NULL, buffer, sizeof(buffer)), 1);
    TEST_STR_EQ(buffer, "");

    buffer[0] = 'X';
    TEST_INT_EQ(capture_line("false", NULL, buffer, sizeof(buffer)), 1);
    TEST_STR_EQ(buffer, "");

    buffer[0] = 'X';
    TEST_INT_EQ(capture_line("echo", "", buffer, sizeof(buffer)), 1);
    TEST_STR_EQ(buffer, "");

    buffer[0] = 'X';
    TEST_INT_EQ(capture_line("cat", NULL, buffer, sizeof(buffer)), 1);
    TEST_STR_EQ(buffer, "");
}

static void test_capture_failures(void) {
    char buffer[32] = {'X'};

    TEST_INT_EQ(capture_line("textfetch-command-that-does-not-exist", NULL,
                             buffer, sizeof(buffer)), 1);
    TEST_STR_EQ(buffer, "");

    TEST_INT_EQ(capture_line(NULL, NULL, buffer, sizeof(buffer)), -1);
    TEST_INT_EQ(capture_line("echo", "hello", NULL, sizeof(buffer)), -1);

    buffer[0] = 'X';
    TEST_INT_EQ(capture_line("echo", "hello", buffer, 0), -1);
    TEST_STR_EQ(buffer, "X");
}

int main(void) {
    test_successful_capture();
    test_truncation();
    test_empty_output();
    test_capture_failures();

    return test_finish("capture");
}
