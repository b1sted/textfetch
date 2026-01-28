/* SPDX-License-Identifier: MIT */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define APP_VERSION "1.0-alpha"

/**
 * Checks if verbose logging is enabled.
 * @return true if the --verbose flag was provided.
 */
bool cfg_is_verbose(void);

/**
 * Checks if color output should be used.
 * Default is true if stdout is a TTY, unless overridden by --no-color.
 * @return true if colors are enabled.
 */
bool cfg_is_color(void);

/**
 * Parses command line arguments and initializes global settings.
 * Handles immediate exit flags like --help and --version.
 * 
 * @param argc Argument count from main.
 * @param argv Argument vector from main.
 */
void cfg_init(int argc, char *argv[]);

#endif // CONFIG_H