/* SPDX-License-Identifier: MIT */

#include <stdio.h>

#include "hardware.h"
#include "system.h"
#include "terminal.h"
#include "ui.h"

int main(void) {
    ui_init();
    
    system_init();
    system_print_header();
    system_print_info();
    
    terminal_print_info();

    hardware_print_info();

    return 0;
}
