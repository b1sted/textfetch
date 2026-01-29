/* SPDX-License-Identifier: MIT */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define APP_VERSION "1.0-alpha"

/**
 * Checks if force kibibytes (KiB) output is enabled.
 * @return true if the -k or --kibibytes flag was provided.
 */
bool cfg_is_kib(void);

/**
 * Checks if force mebibytes (MiB) output is enabled.
 * @return true if the -m or --mebibytes flag was provided.
 */
bool cfg_is_mib(void);

/**
 * Checks if force gibibytes (GiB) output is enabled.
 * @return true if the -g or --gibibytes flag was provided.
 */
bool cfg_is_gib(void);

/**
 * Checks if verbose logging is enabled.
 * @return true if the -v or --verbose flag was provided.
 */
bool cfg_is_verbose(void);

/**
 * Checks if color output is enabled.
 * 
 * Logic:
 * - Enabled by default if stdout is a TTY.
 * - Automatically disabled if the NO_COLOR environment variable is set.
 * - Can be explicitly disabled via the --no-color command-line flag.
 * 
 * @return true if colors should be used for output.
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