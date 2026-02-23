/* SPDX-License-Identifier: MIT */

#ifndef HARDWARE_H
#define HARDWARE_H

/**
 * Orchestrates the detection and display of all hardware components.
 * Gathers data for CPU, GPU, RAM, Swap, Disks, and Battery,
 * then routes formatted output to the UI module.
 */
void hardware_print_info(void);

#endif /* HARDWARE_H */