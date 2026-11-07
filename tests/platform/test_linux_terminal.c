/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include "pal/terminal_os.h"

#include <stdio.h>
#include <string.h>

#include "../support/test_framework.h"
#include "../support/test_helpers.h"
#include "defs.h"
#include "terminal.h"

/* Include the translation unit to exercise its static helpers directly. */
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../../src/platform/linux-common/terminal.c"

static void test_shell_names(void)
{
    TEST_CHECK(term_is_shell_name("sh"));
    TEST_CHECK(term_is_shell_name("bash"));
    TEST_CHECK(term_is_shell_name("zsh"));
    TEST_CHECK(term_is_shell_name("fish"));
    TEST_CHECK(term_is_shell_name("BASH"));

    TEST_CHECK(!term_is_shell_name(""));
    TEST_CHECK(!term_is_shell_name(NULL));
    TEST_CHECK(!term_is_shell_name("linker64"));
    TEST_CHECK(!term_is_shell_name("tmux"));
    TEST_CHECK(!term_is_shell_name("sudo"));
}

static void test_process_stat(void)
{
    char name[TINY_BUFFER] = {0};
    pid_t ppid = 0;

    TEST_CHECK(term_read_process_stat(getpid(), name, sizeof(name), &ppid));
    TEST_CHECK(strlen(name) > 0);
    TEST_INT_EQ(ppid, getppid());

    TEST_CHECK(!term_read_process_stat(getpid(), name, 0, &ppid));
    TEST_CHECK(!term_read_process_stat(getpid(), NULL, sizeof(name), &ppid));
    TEST_CHECK(!term_read_process_stat(getpid(), name, sizeof(name), NULL));
    TEST_CHECK(!term_read_process_stat(-1, name, sizeof(name), &ppid));
}

static void test_cmdline(void)
{
    char name[TINY_BUFFER] = {0};
    bool interactive = false;

    TEST_CHECK(term_read_cmdline(getpid(), name, sizeof(name), &interactive));
    TEST_CHECK(strlen(name) > 0);

    TEST_CHECK(!term_read_cmdline(getpid(), name, 0, &interactive));
    TEST_CHECK(!term_read_cmdline(getpid(), NULL, sizeof(name), &interactive));
    TEST_CHECK(!term_read_cmdline(getpid(), name, sizeof(name), NULL));
    TEST_CHECK(!term_read_cmdline(-1, name, sizeof(name), &interactive));
}

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
    test_shell_names();
    test_process_stat();
    test_cmdline();
    test_get_shell();
    test_print_info();

    return test_finish("linux_terminal");
}
