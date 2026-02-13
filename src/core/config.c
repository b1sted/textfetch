/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <getopt.h>
#include <unistd.h>

#include "config.h"
#include "ui.h"

const char version_string[] __attribute__((used, section(VERSION_SECTION))) = APP_VERSION;

static struct {
    unsigned int kib_format : 1;
    unsigned int mib_format : 1;
    unsigned int gib_format : 1;
    unsigned int verbose    : 1;
    unsigned int color      : 1;
} settings = {0, 0, 0, 0, 0};

bool cfg_is_kib(void) { return settings.kib_format; }

bool cfg_is_mib(void) { return settings.mib_format; }

bool cfg_is_gib(void) { return settings.gib_format; }

bool cfg_is_verbose(void) { return settings.verbose; }

bool cfg_is_color(void) { return settings.color; }

void cfg_init(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"human-readable", no_argument, NULL, 'h'},
        {"kibibytes", no_argument, NULL, 'k'},
        {"mebibytes", no_argument, NULL, 'm'},
        {"gibibytes", no_argument, NULL, 'g'},
        {"verbose", no_argument, NULL, 'v'},
        {"no-color", no_argument, NULL, 1001},
        {"help", no_argument, NULL, 1002},
        {"version", no_argument, NULL, 'V'},
        {0, 0, 0, 0}
    };

    if (isatty(STDOUT_FILENO) && getenv("NO_COLOR") == NULL) settings.color = true;

    int opt;
    while ((opt = getopt_long(argc, argv, "hkmgvV", long_options, NULL)) != -1) {
        switch (opt) {
        case 'h':
            break;
        case 'k':
            settings.kib_format = true;
            break;
        case 'm':
            settings.mib_format = true;
            break;
        case 'g':
            settings.gib_format = true;
            break;
        case 'v':
            settings.verbose = true;
            break;
        case 1001:
            settings.color = false;
            break;
        case 1002:
            ui_print_help(argv[0]);
            exit(EXIT_SUCCESS);
        case 'V':
            ui_print_version(APP_VERSION);
            exit(EXIT_SUCCESS);
        default:
            fprintf(stderr, "Try '%s --help' for more information\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}
