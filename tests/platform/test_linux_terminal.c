/* SPDX-License-Identifier: MIT */

#include "pal/terminal_os.h"

#include <stdio.h>
#include <string.h>

#include "../support/test_framework.h"
#include "../support/test_helpers.h"
#include "defs.h"
#include "terminal.h"

static void test_get_shell(void)
{
    char buffer[LINE_BUFFER] = {0};

    setenv("SHELL", "/bin/zsh", 1);
    term_get_shell(buffer, sizeof(buffer));
    TEST_CHECK(strlen(buffer) > 0);
    TEST_STR_NOT_CONTAINS(buffer, "\n");

    memset(buffer, 0, sizeof(buffer));
    buffer[0] = 'X';
    term_get_shell(buffer, 0);
    TEST_STR_EQ(buffer, "X");

    term_get_shell(NULL, sizeof(buffer));
}

static void test_print_info(void)
{
    test_capture_t capture = {0};
    TEST_CHECK(test_capture_begin(&capture));

    term_print_info();

    char *output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    TEST_STR_CONTAINS(output, "Shell: ");
    TEST_STR_CONTAINS(output, "Locale: ");
    free(output);
}

int main(void)
{
    test_get_shell();
    test_print_info();

    return test_finish("linux_terminal");
}
