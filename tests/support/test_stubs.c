/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <string.h>

#include "pal/hardware_os.h"
#include "pal/system_os.h"
#include "test_stubs.h"

char stub_identity[STUB_BUFFER_SIZE] = "test-user";
char stub_distro[STUB_BUFFER_SIZE] = "TestOS x86_64";
char stub_model[STUB_BUFFER_SIZE] = "Test Device";
char stub_uptime[STUB_BUFFER_SIZE] = "1 days, 02:03:04";
char stub_hardware_calls[64] = "";

void stubs_reset(void)
{
    snprintf(stub_identity, sizeof(stub_identity), "%s", "test-user");
    snprintf(stub_distro, sizeof(stub_distro), "%s", "TestOS x86_64");
    snprintf(stub_model, sizeof(stub_model), "%s", "Test Device");
    snprintf(stub_uptime, sizeof(stub_uptime), "%s", "1 days, 02:03:04");
    stub_hardware_calls[0] = '\0';
}

void sys_get_identity(char *out_buf, const size_t buf_size)
{
    if (!out_buf || buf_size == 0) {
        return;
    }
    snprintf(out_buf, buf_size, "%s", stub_identity);
}

void sys_get_distro(char *out_buf, const size_t buf_size)
{
    if (!out_buf || buf_size == 0) {
        return;
    }
    snprintf(out_buf, buf_size, "%s", stub_distro);
}

void sys_get_model_name(char *out_buf, const size_t buf_size)
{
    if (!out_buf || buf_size == 0) {
        return;
    }
    snprintf(out_buf, buf_size, "%s", stub_model);
}

void sys_format_uptime(char *out_buf, const size_t buf_size)
{
    if (!out_buf || buf_size == 0) {
        return;
    }
    snprintf(out_buf, buf_size, "%s", stub_uptime);
}

static void record_hardware_call(const char *name)
{
    size_t used = strlen(stub_hardware_calls);

    if (used > 0) {
        snprintf(stub_hardware_calls + used, sizeof(stub_hardware_calls) - used, ",%s", name);
    } else {
        snprintf(stub_hardware_calls, sizeof(stub_hardware_calls), "%s", name);
    }
}

void hw_get_cpu_info(void) { record_hardware_call("cpu"); }

void hw_get_gpu_info(void) { record_hardware_call("gpu"); }

void hw_get_mem_info(void) { record_hardware_call("mem"); }

void hw_get_drives_info(void) { record_hardware_call("drives"); }

void hw_get_bat_info(void) { record_hardware_call("battery"); }
