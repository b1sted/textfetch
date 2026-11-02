/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <sys/system_properties.h>

#include "defs.h"
#include "sys_utils.h"

#include "pal/system_os.h"

void sys_get_distro(char *out_buf, const size_t buf_size) {
    char android_version[PROP_VALUE_MAX] = "Unknown";

    if (__system_property_get("ro.build.version.release", android_version) <= 0) {
        V_PRINTF("[WARNING] Could not read version, using 'Unknown'\n");
        snprintf(android_version, sizeof(android_version), "%s", "Unknown");
    }

    snprintf(out_buf, buf_size, "Android %s %s", android_version, sys_data.machine);
}

void sys_get_model_name(char *out_buf, size_t buf_size) {
    char brand_buf[PROP_VALUE_MAX] = {0};
    char model_buf[PROP_VALUE_MAX] = {0};
    char name_buf[PROP_VALUE_MAX] = {0};

    const char *fallback = "Unknown";

    if (__system_property_get("ro.product.brand", brand_buf) <= 0) {
        V_PRINTF("[WARNING] Could not read brand, using 'Unknown'\n");
        snprintf(brand_buf, sizeof(brand_buf), "%s", fallback);
    }

    if (__system_property_get("ro.product.model", model_buf) <= 0) {
        V_PRINTF("[WARNING] Could not read model, using 'Unknown'\n");
        snprintf(model_buf, sizeof(model_buf), "%s", fallback);
    }

    if (__system_property_get("ro.product.name", name_buf) <= 0) {
        V_PRINTF("[WARNING] Could not read name, using 'Unknown'\n");
        snprintf(name_buf, sizeof(name_buf), "%s", fallback);
    }

    bool brand_in_model = (strstr(model_buf, brand_buf) != NULL);
    bool model_is_name  = (strcmp(model_buf, name_buf) == 0);
    bool brand_is_model = (strcmp(brand_buf, model_buf) == 0);

    if (brand_is_model) {
        snprintf(out_buf, buf_size, "%s", model_buf);
    } else if (brand_in_model) {
        snprintf(out_buf, buf_size, "%s (%s)", model_buf, name_buf);
    } else if (model_is_name) {
        snprintf(out_buf, buf_size, "%s %s", brand_buf, model_buf);
    } else {
        snprintf(out_buf, buf_size, "%s %s (%s)", brand_buf, model_buf, name_buf);
    }
}
