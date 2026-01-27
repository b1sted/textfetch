/* SPDX-License-Identifier: MIT */

#ifndef UI_H
#define UI_H

#define ANSI_BOLD            "\x1b[1m"
#define ANSI_RESET           "\x1b[0m"

/**
 * Initializes the user interface module.
 * Detects terminal capabilities (TTY) to determine whether ANSI color 
 * formatting should be enabled for all subsequent output.
 */
void ui_init(void);

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

#endif // UI_H