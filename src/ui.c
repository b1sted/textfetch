/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include "ui.h"

void ui_render_header(const char *username, const char *nodename) {
    if (cfg_is_color()) {
        printf(ANSI_BOLD "%s@%s\n" ANSI_RESET, username, nodename);
    } else {
        printf("%s@%s\n", username, nodename);
    }

    int print_len = strlen(username) + strlen(nodename) + 1;

    for (int i = 0; i < print_len; i++) putchar('-');
    printf("\n");
}

void ui_print_info(const char *label, const char *information) {
    if (cfg_is_color()) {
        printf(ANSI_BOLD "%s: " ANSI_RESET, label);
    } else {
        printf("%s: ", label);
    }

    printf("%s\n", information);
}

void ui_print_help(const char *prog_name) {
    if (cfg_is_color()) {
        printf(ANSI_BOLD "Usage: " ANSI_RESET);
        printf("%s [OPTIONS]\n\n", prog_name);
        printf(ANSI_BOLD "Options:\n" ANSI_RESET);
    } else {
        printf("Usage: %s [OPTIONS]\n\n", prog_name);
        printf("Options:\n");
    }
    
    printf("  -h, --help            Show this help message and exit.\n");
    printf("  -V, --version         Show program's version number and exit.\n");
    printf("  -v, --verbose         Enable verbose output (logs sent to stderr).\n");
    printf("  --no-color            Disable colored output.\n");
}

void ui_print_version(const char *app_version) {
    printf("textfetch v%s\n", app_version);
}