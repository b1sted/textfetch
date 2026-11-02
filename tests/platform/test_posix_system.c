/* SPDX-License-Identifier: MIT */

#include "pal/system_os.h"

#include <string.h>

#include "../support/test_framework.h"

/* The platform sources only need the shared data block. */
sys_internal_data_t sys_data;

static void test_format_uptime(void)
{
    char buffer[64];

    sys_data.uptime = 0;
    sys_format_uptime(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "00:00:00");

    sys_data.uptime = 59;
    sys_format_uptime(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "00:00:59");

    sys_data.uptime = 60;
    sys_format_uptime(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "00:01:00");

    sys_data.uptime = 3599;
    sys_format_uptime(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "00:59:59");

    sys_data.uptime = 3600;
    sys_format_uptime(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "01:00:00");

    sys_data.uptime = 86399;
    sys_format_uptime(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "23:59:59");

    sys_data.uptime = 86400;
    sys_format_uptime(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "1 days, 00:00:00");

    sys_data.uptime = 90061;
    sys_format_uptime(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "1 days, 01:01:01");

    sys_data.uptime = 2678400;
    sys_format_uptime(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "31 days, 00:00:00");

    sys_data.uptime = 123456789;
    sys_format_uptime(buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "1428 days, 21:33:09");
}

static void test_identity(void)
{
    char buffer[256] = {0};

    sys_get_identity(buffer, sizeof(buffer));
    TEST_CHECK(strlen(buffer) > 0);
    TEST_STR_NOT_CONTAINS(buffer, "\n");

    memset(buffer, 0, sizeof(buffer));
    buffer[0] = 'X';
    sys_get_identity(buffer, 0);
    TEST_STR_EQ(buffer, "X");

    sys_get_identity(NULL, sizeof(buffer));
}

int main(void)
{
    test_format_uptime();
    test_identity();

    return test_finish("posix_system");
}
