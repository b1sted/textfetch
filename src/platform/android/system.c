/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <sys/system_properties.h>

#include "defs.h"
#include "sys_utils.h"

#include "pal/system_os.h"

void sys_get_distro(char *out_buf, const size_t buf_size) {
    char android_version[PROP_NAME_MAX] = "Unknown";

    if (__system_property_get("ro.build.version.release", android_version) <= 0) {
        V_PRINTF("Warning: Could not read version, using 'Unknown'\n");
    }

    snprintf(out_buf, buf_size, "Android %s %s", android_version, sys_data.machine);
}

void sys_get_model_name(char *out_buf, size_t buf_size) {
    char brand_buf[PROP_NAME_MAX] = {0};
    char model_buf[PROP_NAME_MAX] = {0};
    char name_buf[PROP_NAME_MAX] = {0};

    const char *fallback = "Unknown";

    if (__system_property_get("ro.product.brand", brand_buf) <= 0) {
        V_PRINTF("Warning: Could not read version, using 'Unknown'\n");
        strcpy(brand_buf, fallback);
    }

    if (__system_property_get("ro.product.model", model_buf) <= 0) {
        V_PRINTF("Warning: Could not read version, using 'Unknown'\n");
        strcpy(model_buf, fallback);
    }

    if (__system_property_get("ro.product.name", name_buf) <= 0) {
        V_PRINTF("Warning: Could not read version, using 'Unknown'\n");
        strcpy(name_buf, fallback);
    }

    bool brand_in_model = (strstr(model_buf, brand_buf) != NULL);
    bool model_is_name = (strcmp(model_buf, name_buf) == 0);
    bool brand_is_model = (strcmp(brand_buf, model_buf) == 0);

    if ((brand_is_model || brand_in_model) && brand_is_model) {
        snprintf(out_buf, buf_size, "%s", model_buf);
    } else if (brand_is_model || brand_in_model) {
        snprintf(out_buf, buf_size, "%s (%s)", model_buf, name_buf);
    } else if (model_is_name) {
        snprintf(out_buf, buf_size, "%s %s", brand_buf, model_buf);
    } else {
        snprintf(out_buf, buf_size, "%s %s (%s)", brand_buf, model_buf, name_buf);
    }
}
