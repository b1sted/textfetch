/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pal/terminal_os.h"
#include "terminal.h"
#include "../support/test_framework.h"
#include "../support/test_helpers.h"

/* Deterministic shell value used by terminal_print_info(). */
void term_get_shell(char *out_buf, const size_t buf_size) {
    if (!out_buf || buf_size == 0) return;
    snprintf(out_buf, buf_size, "%s", "bash 5.2.15");
}

static void test_fallback_shell(void) {
    char buffer[64];

    setenv("SHELL", "/bin/zsh", 1);
    term_fallback_shell(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "zsh");

    setenv("SHELL", "/usr/local/bin/fish", 1);
    term_fallback_shell(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "fish");

    setenv("SHELL", "bash", 1);
    term_fallback_shell(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "bash");

    setenv("SHELL", "", 1);
    term_fallback_shell(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "sh");

    unsetenv("SHELL");
    term_fallback_shell(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "sh");

    setenv("SHELL", "/bin/bash", 1);
    term_fallback_shell(NULL, sizeof(buffer));
    term_fallback_shell(buffer, 0);
}

static void test_sanitize_name(void) {
    char buffer[128];

    snprintf(buffer, sizeof(buffer), "%s",
             "GNU bash, version 5.2.15(1)-release (x86_64-pc-linux-gnu)");
    term_sanitize_name(buffer);
    TEST_STR_EQ(buffer, "bash 5.2.15");

    snprintf(buffer, sizeof(buffer), "%s",
             "GNU bash, version 5.1.16(1)-release");
    term_sanitize_name(buffer);
    TEST_STR_EQ(buffer, "bash 5.1.16");

    snprintf(buffer, sizeof(buffer), "%s",
             "zsh 5.9 (x86_64-apple-darwin23.0)");
    term_sanitize_name(buffer);
    TEST_STR_EQ(buffer, "zsh 5.9");

    snprintf(buffer, sizeof(buffer), "%s", "fish, version 3.6.1");
    term_sanitize_name(buffer);
    TEST_STR_EQ(buffer, "fish 3.6.1");

    snprintf(buffer, sizeof(buffer), "%s", "GNU nano 7.2");
    term_sanitize_name(buffer);
    TEST_STR_EQ(buffer, "nano 7.2");

    snprintf(buffer, sizeof(buffer), "%s", "sh");
    term_sanitize_name(buffer);
    TEST_STR_EQ(buffer, "sh");

    snprintf(buffer, sizeof(buffer), "%s", "dash 0.5.12-2ubuntu1");
    term_sanitize_name(buffer);
    TEST_STR_EQ(buffer, "dash 0.5.12-2ubuntu1");

    snprintf(buffer, sizeof(buffer), "%s", "tcsh 6.24.00 (Astron)");
    term_sanitize_name(buffer);
    TEST_STR_EQ(buffer, "tcsh 6.24.00");

    snprintf(buffer, sizeof(buffer), "%s", "Python 3.11.2");
    term_sanitize_name(buffer);
    TEST_STR_EQ(buffer, "Python 3.11.2");

    buffer[0] = '\0';
    term_sanitize_name(buffer);
    TEST_STR_EQ(buffer, "");

    term_sanitize_name(NULL);
}

static void test_print_info(void) {
    test_capture_t capture;
    TEST_CHECK(test_capture_begin(&capture));

    terminal_print_info();

    char *output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    TEST_STR_CONTAINS(output, "Shell: bash 5.2.15\n");
    TEST_STR_CONTAINS(output, "Locale: ");
    free(output);
}

int main(void) {
    test_fallback_shell();
    test_sanitize_name();
    test_print_info();

    return test_finish("terminal");
}
