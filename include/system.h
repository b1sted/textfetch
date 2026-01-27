/* SPDX-License-Identifier: MIT */

#ifndef SYSTEM_H
#define SYSTEM_H

/**
 * Initializes the system module data.
 * Caches kernel information (uname) and system statistics (sysinfo) 
 * into internal static structures to avoid redundant system calls.
 */
void system_init(void);

/**
 * Orchestrates the rendering of the user@hostname header.
 * Fetches the current identity and system node name, then triggers 
 * the UI module to display the formatted header.
 */
void system_print_header(void);

/**
 * Aggregates and prints a block of general system information.
 * Displays OS distribution, kernel release, uptime, and process count 
 * using standardized UI formatting.
 */
void system_print_info(void);

#endif // SYSTEM_H
