/* SPDX-License-Identifier: MIT */

#include "config.h"
#include "hardware.h"
#include "system.h"
#include "terminal.h"

int main(int argc, char *argv[]) {
    cfg_init(argc, argv);

    system_init();
    system_print_header();
    system_print_info();

    terminal_print_info();

    hardware_print_info();

    return 0;
}
