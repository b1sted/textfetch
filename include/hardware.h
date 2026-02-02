/* SPDX-License-Identifier: MIT */

#ifndef HARDWARE_H
#define HARDWARE_H

#define HZ_PER_GHZ           1e9

#define BYTES_TO_KIB_DIVISOR 1024
#define BYTES_TO_MIB_DIVISOR (1024.0 * 1024.0)
#define BYTES_TO_GIB_DIVISOR (1024.0 * 1024.0 * 1024.0)

/**
 * Orchestrates the detection and display of all hardware components.
 * Gathers data for CPU, GPU, RAM, Swap, Disks, and Battery, 
 * then routes formatted output to the UI module.
 */
void hardware_print_info(void);

#endif // HARDWARE_H