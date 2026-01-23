#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pwd.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/utsname.h>

#define BUFFER_SIZE 512
#define ANSI_BOLD   "\x1b[1m"
#define ANSI_RESET  "\x1b[0m"

/**
 * Renders the user@hostname header with an underline separator.
 * Colors are applied only if the output is a TTY.
 *
 * @param username Current user's login name.
 * @param nodename System hostname.
 * @param is_a_tty Flag indicating if stdout is a terminal.
 */
void print_header(char *username, char *nodename, const bool is_a_tty);

/**
 * Prints a labeled system property formatted nicely.
 * Example output: "OS: Debian GNU/Linux"
 *
 * @param label The property name (e.g., "OS", "Kernel").
 * @param information The property value.
 * @param is_a_tty Flag to enable/disable ANSI colors.
 */
void print_information(char *label, char *information, const bool is_a_tty);

/**
 * Attempts to retrieve the distribution name from standard release files.
 * Prioritizes PRETTY_NAME, falls back to NAME.
 *
 * @param dest_buffer Buffer to store the result string.
 * @param dest_len Size of the destination buffer.
 * @return 0 on success (name found), -1 if files cannot be read.
 */
int get_distro_name(char *dest_buffer, size_t dest_len);

int main(void) {
    struct utsname machine_info;

    if (uname(&machine_info) != 0) {
        perror("uname");
        return 1;
    }

    struct passwd *user_info = getpwuid(geteuid());

    if (!user_info) {
        perror("getpwuid");
        return 1;
    }
    
    char *nodename = machine_info.nodename;
    char *username = user_info->pw_name;

    const bool is_a_tty = isatty(STDOUT_FILENO);

    char distro_name[BUFFER_SIZE] = {0};

    if (get_distro_name(distro_name, BUFFER_SIZE) != 0) {
        fprintf(stderr, "Failed to read os-release\n");
        return 1;
    }

    int size_distro_name = sizeof(distro_name);
    snprintf(distro_name + strlen(distro_name), size_distro_name - strlen(distro_name), " %s", machine_info.machine);

    print_header(username, nodename, is_a_tty);
    print_information("OS", distro_name, is_a_tty);
    print_information("Kernel", machine_info.release, is_a_tty);

    return 0;
}

void print_header(char *username, char *nodename, const bool is_a_tty) {
    int print_len = strlen(username) + strlen(nodename) + 1;

    if (is_a_tty) {
        printf(ANSI_BOLD "%s@%s\n" ANSI_RESET, username, nodename);
    } else {
        printf("%s@%s\n", username, nodename);
    }

    for (int i = 0; i < print_len; i++) putchar('-');
    printf("\n");
}

void print_information(char *label, char *information, const bool is_a_tty) {
    if (is_a_tty) {
        printf(ANSI_BOLD "%s: " ANSI_RESET, label);
    } else {
        printf("%s: ", label);
    }

    printf("%s\n", information);
}

int get_distro_name(char *dest_buffer, size_t dest_len) {
    FILE *release_file = fopen("/etc/os-release", "r");

    if (!release_file) {
        release_file = fopen("/usr/lib/os-release", "r");

        if (!release_file) {
            perror("fail to open os-release");
            return -1;
        }
    }

    char file_line[BUFFER_SIZE];
    
    while (fgets(file_line, sizeof(file_line), release_file)) {
        char *delimiter_ptr = strchr(file_line, '=');
        if (!delimiter_ptr) continue;

        *delimiter_ptr = 0;
        char *key = file_line;
        char *value = delimiter_ptr + 1;

        value[strcspn(value, "\n")] = 0;

        if (value[0] == '"') {
            value++;

            size_t len = strlen(value);

            if (len > 0 && value[len - 1] == '"') {
                value[len - 1] = 0;
            }
        }

        if (strcmp(key, "NAME") == 0) {
            snprintf(dest_buffer, dest_len, "%s", value);
        }

        if (strcmp(key, "PRETTY_NAME") == 0) {
            snprintf(dest_buffer, dest_len, "%s", value);
            break;
        }
    }

    fclose(release_file);

    return 0;
}