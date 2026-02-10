/* SPDX-License-Identifier: MIT */

#ifndef UI_H
#define UI_H

#include "config.h"

#define TINY_BUFFER   16
#define MINI_BUFFER   32
#define SMALL_BUFFER  64
#define MEDIUM_BUFFER 128
#define LINE_BUFFER   256
#define PATH_BUFFER   512

/**
 * Verbose perror wrapper.
 * Only prints the system error message if the verbose flag is set.
 */
#define V_PERROR(msg) \
    do { if (cfg_is_verbose()) perror(msg); } while (0)

/**
 * Verbose formatted print wrapper.
 * Sends output to stderr only if the verbose flag is set.
 */
#define V_PRINTF(fmt, ...) \
    do { if (cfg_is_verbose()) fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

/**
 * Renders the user@hostname header with an underline separator.
 * Colors are applied only if the output is a TTY.
 *
 * @param username Current user's login name.
 * @param nodename System hostname.
 */
void ui_render_header(const char *username, const char *nodename);

/**
 * Prints a labeled system property formatted nicely.
 * Example output: "OS: Debian GNU/Linux"
 *
 * @param label The property name (e.g., "OS", "Kernel").
 * @param information The property value.
 */
void ui_print_info(const char *label, const char *information);

/**
 * Displays the usage instructions and available command-line options.
 * Automatically adjusts formatting based on the global color configuration.
 *
 * @param prog_name The name of the executable, typically passed from argv[0].
 */
void ui_print_help(const char *prog_name);

/**
 * Displays the current program version information.
 *
 * @param app_version The version string to display (e.g., "1.0-alpha").
 */
void ui_print_version(const char *app_version);

#endif // UI_H