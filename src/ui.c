/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include "ui.h"

static bool use_color;

void ui_init(void) {
    use_color = isatty(STDOUT_FILENO);
}

void ui_render_header(const char *username, const char *nodename) {
    if (use_color) {
        printf(ANSI_BOLD "%s@%s\n" ANSI_RESET, username, nodename);
    } else {
        printf("%s@%s\n", username, nodename);
    }

    int print_len = strlen(username) + strlen(nodename) + 1;

    for (int i = 0; i < print_len; i++) putchar('-');
    printf("\n");
}

void ui_print_info(const char *label, const char *information) {
    if (use_color) {
        printf(ANSI_BOLD "%s: " ANSI_RESET, label);
    } else {
        printf("%s: ", label);
    }

    printf("%s\n", information);
}