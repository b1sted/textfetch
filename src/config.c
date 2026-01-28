/* SPDX-License-Identifier: MIT */

#include <stdlib.h>
#include <stdio.h>

#include <getopt.h>
#include <unistd.h>

#include "config.h"
#include "ui.h"

static struct {
    unsigned int verbose : 1;
    unsigned int color   : 1;
} settings = {0, 0};

bool cfg_is_verbose(void) { return settings.verbose; }

bool cfg_is_color(void) { return settings.color; }

void cfg_init(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"help",     no_argument, NULL, 'h'},
        {"version",  no_argument, NULL, 'V'},
        {"verbose",  no_argument, NULL, 'v'},
        {"no-color", no_argument, NULL, 1001},
        {0, 0, 0, 0}
    };

    if (isatty(STDOUT_FILENO)) settings.color = true;

    int opt;
    while ((opt = getopt_long(argc, argv, "hVv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':  ui_print_help(argv[0]); exit(EXIT_SUCCESS);
            case 'V':  ui_print_version(APP_VERSION); exit(EXIT_SUCCESS);
            case 'v':  settings.verbose = true; break;
            case 1001: settings.color = false; break;
            default: 
                fprintf(stderr, "Try '%s --help' for more information\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
}