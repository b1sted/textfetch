/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <stdlib.h>

#include <getopt.h>
#include <unistd.h>

#include "config.h"
#include "hardware.h"
#include "system.h"
#include "terminal.h"
#include "ui.h"

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"human-readable", no_argument, NULL, 'h'},
        {"kibibytes",      no_argument, NULL, 'k'},
        {"mebibytes",      no_argument, NULL, 'm'},
        {"gibibytes",      no_argument, NULL, 'g'},
        {"verbose",        no_argument, NULL, 'v'},
        {"no-color",       no_argument, NULL, 1001},
        {"help",           no_argument, NULL, 1002},
        {"version",        no_argument, NULL, 'V'},
        {0, 0, 0, 0}
    };

    if (isatty(STDOUT_FILENO) && getenv("NO_COLOR") == NULL) cfg_set_color(true);

    int opt;
    while ((opt = getopt_long(argc, argv, "hkmgvV", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                break;
            case 'k':
                cfg_set_kib();
                break;
            case 'm':
                cfg_set_mib();
                break;
            case 'g':
                cfg_set_gib();
                break;
            case 'v':
                cfg_set_verbose();
                break;
            case 1001:
                cfg_set_color(false);
                break;
            case 1002:
                ui_print_help(argv[0]);
                exit(EXIT_SUCCESS);
            case 'V':
                ui_print_version(APP_VERSION);
                exit(EXIT_SUCCESS);
            default:
                fprintf(stderr, 
                        "[ERROR] Unknown option. Try '%s --help' for more information\n",
                        argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    system_init();
    system_print_header();
    system_print_info();

    terminal_print_info();

    hardware_print_info();

    return 0;
}
