/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <string.h>

#include <errno.h>

#include <sys/utsname.h>

#include "ui.h"
#include "sys_utils.h"

#define COLOR_BOLD "\x1b[1m"
#define COLOR_RESET "\x1b[0m"

static const char *get_color(const char *ansi_code);

void ui_render_header(const char *username, const char *nodename) {
    printf("%s%s@%s%s\n", get_color(COLOR_BOLD), username, nodename, 
           get_color(COLOR_RESET));

    int print_len = strlen(username) + strlen(nodename) + 1;

    for (int i = 0; i < print_len; i++) putchar('-');
    printf("\n");
}

void ui_print_info(const char *label, const char *information) {
    printf("%s%s%s: %s\n", get_color(COLOR_BOLD), label, get_color(COLOR_RESET), 
           information);
}

void ui_print_help(const char *prog_name) {
    printf("%sUsage:%s %s [OPTIONS]\n\n", get_color(COLOR_BOLD), get_color(COLOR_RESET), 
           prog_name);
    printf("%sOutput Formatting:%s\n", get_color(COLOR_BOLD), get_color(COLOR_RESET));
    printf("  -h, --human-readable   print sizes in human readable format "
           "(default)\n");
    printf("  -k, --kibibytes        force kibibytes\n");
    printf("  -m, --mebibytes        force mebibytes\n");
    printf("  -g, --gibibytes        force gibibytes\n\n");
    printf("%sOptions:%s\n", get_color(COLOR_BOLD), get_color(COLOR_RESET));
    printf("  -v, --verbose          explain what is being done\n");
    printf("      --no-color         disable color output\n\n");
    printf("%sSystem:%s\n", get_color(COLOR_BOLD), get_color(COLOR_RESET));
    printf("      --help             display this help and exit\n");
    printf("  -V, --version          output version information and exit\n");
}

void ui_print_version(const char *app_version) {
    struct utsname sys_info;
    memset(&sys_info, 0, sizeof(struct utsname));

    if (uname(&sys_info) != 0) {
        V_PRINTF("Error: uname failed: %s\n", strerror(errno));
    }

    if (strlen(sys_info.machine) > 0) {
        printf("textfetch v%s (%s)\n", app_version, sys_info.machine);
    } else {
        printf("textfetch v%s\n", app_version);
    }
}

static const char *get_color(const char *ansi_code) { 
    return cfg_is_color() ? ansi_code : ""; 
}
