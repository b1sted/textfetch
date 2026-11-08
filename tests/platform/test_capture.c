/* SPDX-License-Identifier: MIT */

#include "capture.h"

#include <stdint.h>
#include <string.h>

#include <dirent.h>
#include <unistd.h>

#include <sys/resource.h>

#include "../support/test_framework.h"

/* Counts open descriptors, excluding the descriptor used to read the directory. */
static int32_t count_open_fds(void)
{
    DIR *dir = opendir("/proc/self/fd");
    if (!dir) {
        return -1;
    }

    int32_t count = 0;
    struct dirent *entry = NULL;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            count++;
        }
    }

    closedir(dir);

    return count - 1;
}

static void test_successful_capture(void)
{
    char buffer[64] = {0};

    TEST_INT_EQ(capture_line("echo", "hello", buffer, sizeof(buffer)), 0);
    TEST_STR_EQ(buffer, "hello");

    memset(buffer, 0, sizeof(buffer));
    TEST_INT_EQ(capture_line("echo", "hello world", buffer, sizeof(buffer)), 0);
    TEST_STR_EQ(buffer, "hello world");

    memset(buffer, 0, sizeof(buffer));
    TEST_INT_EQ(capture_line("printf", "first\\nsecond\\n", buffer, sizeof(buffer)), 0);
    TEST_STR_EQ(buffer, "first");

    memset(buffer, 0, sizeof(buffer));
    TEST_INT_EQ(capture_line("uname", NULL, buffer, sizeof(buffer)), 0);
    TEST_CHECK(strlen(buffer) > 0);
    TEST_STR_NOT_CONTAINS(buffer, "\n");
}

static void test_truncation(void)
{
    char buffer[5] = {'X', 'X', 'X', 'X', 'X'};

    TEST_INT_EQ(capture_line("echo", "0123456789", buffer, sizeof(buffer)), 0);
    TEST_STR_EQ(buffer, "0123");
}

static void test_empty_output(void)
{
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

static void test_capture_failures(void)
{
    char buffer[32] = {'X'};

    TEST_INT_EQ(capture_line("textfetch-command-that-does-not-exist", NULL, buffer, sizeof(buffer)),
                1);
    TEST_STR_EQ(buffer, "");

    TEST_INT_EQ(capture_line(NULL, NULL, buffer, sizeof(buffer)), -1);
    TEST_INT_EQ(capture_line("echo", "hello", NULL, sizeof(buffer)), -1);

    buffer[0] = 'X';
    TEST_INT_EQ(capture_line("echo", "hello", buffer, 0), -1);
    TEST_STR_EQ(buffer, "X");
}

static void test_async_capture(void)
{
    capture_job_t job;
    char buffer[64] = {0};

    capture_job_init(&job);
    TEST_INT_EQ(capture_start(&job, "printf", "first\\nsecond\\n"), 0);
    TEST_INT_EQ(capture_start(&job, "echo", "again"), -1);
    TEST_INT_EQ(capture_finish(&job, buffer, sizeof(buffer)), 0);
    TEST_STR_EQ(buffer, "first");

    capture_job_init(&job);
    memset(buffer, 0, sizeof(buffer));
    TEST_INT_EQ(capture_start(&job, "echo", "hello"), 0);
    TEST_INT_EQ(capture_finish(&job, buffer, sizeof(buffer)), 0);
    TEST_STR_EQ(buffer, "hello");

    capture_job_init(&job);
    TEST_INT_EQ(capture_finish(&job, buffer, sizeof(buffer)), -1);
    TEST_INT_EQ(capture_finish(&job, NULL, sizeof(buffer)), -1);

    capture_job_init(NULL);
    TEST_INT_EQ(capture_start(NULL, "echo", "hello"), -1);
    TEST_INT_EQ(capture_finish(NULL, buffer, sizeof(buffer)), -1);
}

static void test_fork_failure_closes_pipe(void)
{
    struct rlimit original = {0};

    if (getrlimit(RLIMIT_NPROC, &original) != 0) {
        return;
    }
    if (geteuid() == 0) {
        return;
    }

    struct rlimit no_processes = original;
    no_processes.rlim_cur = 0;

    if (setrlimit(RLIMIT_NPROC, &no_processes) != 0) {
        return;
    }

    int32_t before = count_open_fds();
    char buffer[32] = {'X'};
    int result = capture_line("echo", "hello", buffer, sizeof(buffer));
    int32_t after = count_open_fds();

    setrlimit(RLIMIT_NPROC, &original);

    TEST_INT_EQ(result, -1);
    TEST_STR_EQ(buffer, "");

    if (before >= 0 && after >= 0) {
        TEST_INT_EQ(before, after);
    }
}

int main(void)
{
    test_successful_capture();
    test_truncation();
    test_empty_output();
    test_capture_failures();
    test_async_capture();
    test_fork_failure_closes_pipe();

    return test_finish("capture");
}
