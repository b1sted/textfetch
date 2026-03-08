/* SPDX-License-Identifier: MIT */

#include <stdbool.h>

#include "config.h"

const char version_string[] __attribute__((used, section(VERSION_SECTION))) = APP_VERSION;

static struct {
    bool kib_format : 1;
    bool mib_format : 1;
    bool gib_format : 1;
    bool verbose    : 1;
    bool color      : 1;
} settings = {0, 0, 0, 0, 0};

bool cfg_get_kib(void) { return settings.kib_format; }
void cfg_set_kib(void) { settings.kib_format = true; }

bool cfg_get_mib(void) { return settings.mib_format; }
void cfg_set_mib(void) { settings.mib_format = true; }

bool cfg_get_gib(void) { return settings.gib_format; }
void cfg_set_gib(void) { settings.gib_format = true; }

bool cfg_get_verbose(void) { return settings.verbose; }
void cfg_set_verbose(void) { settings.verbose = true; }

bool cfg_get_color(void) { return settings.color; }
void cfg_set_color(bool state) { settings.color = state; }
