/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../support/test_framework.h"
#include "../support/test_helpers.h"

static const char *binary_path = "../bin/textfetch";

static test_command_t run_cli(const char *const arguments[])
{
    char *argv[16];
    size_t count = 0;

    argv[count++] = (char *)binary_path;
    for (size_t i = 0; arguments[i] != NULL && count < 15; i++) {
        argv[count++] = (char *)arguments[i];
    }
    argv[count] = NULL;

    return test_run_command(binary_path, argv);
}

static void test_version(void)
{
    const char *short_args[] = {"-V", NULL};
    const char *long_args[] = {"--version", NULL};

    test_command_t result = run_cli(short_args);
    TEST_INT_EQ(result.status, 0);
    TEST_CHECK(result.output != NULL);
    TEST_STR_CONTAINS(result.output, "textfetch v1.2.2");
    TEST_STR_CONTAINS(result.output, "(");
    TEST_CHECK(result.error != NULL && result.error[0] == '\0');
    test_free_command(&result);

    result = run_cli(long_args);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_CONTAINS(result.output, "textfetch v1.2.2");
    test_free_command(&result);

    const char *extra_args[] = {"--version", "ignored", NULL};
    result = run_cli(extra_args);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_CONTAINS(result.output, "textfetch v1.2.2");
    test_free_command(&result);
}

static void test_help(void)
{
    const char *arguments[] = {"--help", NULL};

    test_command_t result = run_cli(arguments);
    TEST_INT_EQ(result.status, 0);
    TEST_CHECK(result.output != NULL);
    TEST_STR_CONTAINS(result.output, "Usage:");
    TEST_STR_CONTAINS(result.output, binary_path);
    TEST_STR_CONTAINS(result.output, "Output Formatting:");
    TEST_STR_CONTAINS(result.output, "-k, --kibibytes");
    TEST_STR_CONTAINS(result.output, "-m, --mebibytes");
    TEST_STR_CONTAINS(result.output, "-g, --gibibytes");
    TEST_STR_CONTAINS(result.output, "-v, --verbose");
    TEST_STR_CONTAINS(result.output, "--no-color");
    TEST_STR_CONTAINS(result.output, "--help");
    TEST_STR_CONTAINS(result.output, "-V, --version");
    TEST_CHECK(result.error != NULL && result.error[0] == '\0');
    test_free_command(&result);
}

static void test_unknown_options(void)
{
    const char *long_args[] = {"--definitely-not-an-option", NULL};
    const char *short_args[] = {"-Z", NULL};

    test_command_t result = run_cli(long_args);
    TEST_CHECK(result.status != 0);
    TEST_CHECK(result.error != NULL);
    TEST_STR_CONTAINS(result.error, "[ERROR] Unknown option");
    test_free_command(&result);

    result = run_cli(short_args);
    TEST_CHECK(result.status != 0);
    TEST_STR_CONTAINS(result.error, "[ERROR] Unknown option");
    test_free_command(&result);
}

static void test_default_output(void)
{
    const char *arguments[] = {NULL};

    test_command_t result = run_cli(arguments);
    TEST_INT_EQ(result.status, 0);
    TEST_CHECK(result.output != NULL);
    TEST_CHECK(result.error != NULL && result.error[0] == '\0');

    TEST_STR_CONTAINS(result.output, "@");
    TEST_STR_CONTAINS(result.output, "\n-");
    TEST_STR_CONTAINS(result.output, "OS: ");
    TEST_STR_CONTAINS(result.output, "Kernel: ");
    TEST_STR_CONTAINS(result.output, "Uptime: ");
    TEST_STR_CONTAINS(result.output, "Processes: ");
    TEST_STR_CONTAINS(result.output, "Shell: ");
    TEST_STR_CONTAINS(result.output, "Locale: ");
    TEST_STR_CONTAINS(result.output, "CPU: ");
    TEST_STR_NOT_CONTAINS(result.output, "\x1b[");

    test_free_command(&result);
}

static void test_human_readable(void)
{
    const char *short_args[] = {"-h", NULL};
    const char *long_args[] = {"--human-readable", NULL};

    test_command_t result = run_cli(short_args);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_CONTAINS(result.output, "OS: ");
    TEST_STR_NOT_CONTAINS(result.output, "Usage:");
    test_free_command(&result);

    result = run_cli(long_args);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_CONTAINS(result.output, "OS: ");
    TEST_STR_NOT_CONTAINS(result.output, "Usage:");
    test_free_command(&result);
}

static void test_size_flags(void)
{
    const char *kib_short[] = {"-k", NULL};
    const char *mib_short[] = {"-m", NULL};
    const char *gib_short[] = {"-g", NULL};
    const char *kib_long[] = {"--kibibytes", NULL};
    const char *mib_long[] = {"--mebibytes", NULL};
    const char *gib_long[] = {"--gibibytes", NULL};

    test_command_t result = run_cli(kib_short);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_CONTAINS(result.output, "RAM: ");
    TEST_STR_CONTAINS(result.output, "KiB");
    test_free_command(&result);

    result = run_cli(mib_short);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_CONTAINS(result.output, "RAM: ");
    TEST_STR_CONTAINS(result.output, "MiB");
    test_free_command(&result);

    result = run_cli(gib_short);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_CONTAINS(result.output, "RAM: ");
    TEST_STR_CONTAINS(result.output, "GiB");
    test_free_command(&result);

    result = run_cli(kib_long);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_CONTAINS(result.output, "KiB");
    test_free_command(&result);

    result = run_cli(mib_long);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_CONTAINS(result.output, "MiB");
    test_free_command(&result);

    result = run_cli(gib_long);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_CONTAINS(result.output, "GiB");
    test_free_command(&result);
}

static void test_verbose_and_combined_flags(void)
{
    const char *verbose[] = {"-v", NULL};
    const char *combined[] = {"-hkmgv", NULL};
    const char *end_of_options[] = {"--", NULL};

    test_command_t result = run_cli(verbose);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_CONTAINS(result.output, "OS: ");
    test_free_command(&result);

    result = run_cli(combined);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_CONTAINS(result.output, "OS: ");
    test_free_command(&result);

    result = run_cli(end_of_options);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_CONTAINS(result.output, "OS: ");
    test_free_command(&result);
}

static void test_no_color(void)
{
    const char *arguments[] = {"--no-color", NULL};
    const char *no_arguments[] = {NULL};

    test_command_t result = run_cli(arguments);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_NOT_CONTAINS(result.output, "\x1b[");
    test_free_command(&result);

    const char *previous = getenv("NO_COLOR");
    char *saved = previous ? strdup(previous) : NULL;
    setenv("NO_COLOR", "1", 1);

    result = run_cli(no_arguments);
    TEST_INT_EQ(result.status, 0);
    TEST_STR_NOT_CONTAINS(result.output, "\x1b[");
    test_free_command(&result);

    if (saved) {
        setenv("NO_COLOR", saved, 1);
        free(saved);
    } else {
        unsetenv("NO_COLOR");
    }
}

int main(int argc, char *argv[])
{
    if (argc > 1) {
        binary_path = argv[1];
    }

    test_version();
    test_help();
    test_unknown_options();
    test_default_output();
    test_human_readable();
    test_size_flags();
    test_verbose_and_combined_flags();
    test_no_color();

    return test_finish("cli");
}
