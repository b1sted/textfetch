/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h> 

#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>

#include "system.h"
#include "internal/system_os.h"
#include "ui.h"

void sys_get_identity(char *out_buf, const size_t buf_size) {
    if (!out_buf || buf_size == 0) {
        V_PRINTF("[Error] %s: invalid arguments\n", __func__);
        return;
    }

    struct passwd *pwd;
    uid_t uid = geteuid();

    if ((pwd = getpwuid(uid)) == NULL) {
        V_PRINTF("[Error] getpwuid(%u) failed: %s\n", uid, strerror(errno));
        snprintf(out_buf, buf_size, "unknown");
        return;
    }

    snprintf(out_buf, buf_size, "%s", pwd->pw_name);
}

void sys_format_uptime(char *out_buf, const size_t buf_size) {
    uint64_t t = sys_data.uptime;

    uint64_t days = t / 86400;
    t %= 86400;

    uint64_t hours = t / 3600;
    t %= 3600;
    
    uint64_t minutes = t / 60;
    uint64_t seconds = t % 60;

    if (days != 0) {
        snprintf(out_buf, buf_size, 
                 "%" PRIu64 " days, %02" PRIu64 ":%02" PRIu64 ":%02" PRIu64, 
                 days, hours, minutes, seconds);
    } else {
        snprintf(out_buf, buf_size, 
                 "%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64, 
                 hours, minutes, seconds);
    }
}