#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <locale.h>
#include <pwd.h>
#include <unistd.h>

#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

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
void print_header(const char *username, const char *nodename, const bool is_a_tty);

/**
 * Prints a labeled system property formatted nicely.
 * Example output: "OS: Debian GNU/Linux"
 *
 * @param label The property name (e.g., "OS", "Kernel").
 * @param information The property value.
 * @param is_a_tty Flag to enable/disable ANSI colors.
 */
void print_information(const char *label, const char *information, const bool is_a_tty);

/**
 * Attempts to retrieve the distribution name from standard release files.
 * Prioritizes PRETTY_NAME, falls back to NAME.
 *
 * @param dest_buffer Buffer to store the result string.
 * @param dest_len Size of the destination buffer.
 * @return 0 on success (name found), -1 if files cannot be read.
 */
int get_distro_name(char *dest_buffer, const size_t dest_len);

/**
 * Formats the uptime duration into a human-readable string.
 * Output format: "D days, HH:MM:SS" (e.g. "2 days, 10:05:01").
 *
 * @param uptime The time in seconds since system boot.
 * @param dest_buffer Buffer to store the resulting string.
 * @param dest_len Size of the destination buffer to prevent overflow.
 */
void format_uptime(long uptime, char *dest_buffer, const size_t dest_len);

/**
 * Retrieves the version string of the parent process's shell.
 * It identifies the shell binary via /proc, executes it with the '--version' flag,
 * and captures the first line of the standard output.
 *
 * @param ppid The process ID of the parent process (used to determine the shell name).
 * @param dest_buffer The buffer where the resulting version string will be stored.
 * @param dest_len The maximum size of the destination buffer.
 * @return Returns 0 on success, or a non-zero error code on failure.
 */
int get_parent_shell_name(pid_t ppid, char *dest_buffer, const size_t dest_len);

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

    struct sysinfo system_info;

    if (sysinfo(&system_info) != 0) {
        perror("sysinfo");
        return 1;
    }

    char uptime_str[BUFFER_SIZE] = {0};
    format_uptime(system_info.uptime, uptime_str, BUFFER_SIZE);

    char procs_str[BUFFER_SIZE] = {0};
    snprintf(procs_str, sizeof(procs_str), "%u", system_info.procs);

    pid_t parent_pid = getppid();
    char shell_name[BUFFER_SIZE] = {0};
    if (get_parent_shell_name(parent_pid, shell_name, BUFFER_SIZE) != 0) return 1;

    char *locale = setlocale(LC_ALL, "");

    if (!locale) {
        locale = "-";
    }

    print_header(username, nodename, is_a_tty);
    print_information("OS", distro_name, is_a_tty);
    print_information("Kernel", machine_info.release, is_a_tty);
    print_information("Uptime", uptime_str, is_a_tty);
    print_information("Processes", procs_str, is_a_tty);
    print_information("Shell", shell_name, is_a_tty);
    print_information("Locale", locale, is_a_tty);
    return 0;
}

void print_header(const char *username, const char *nodename, const bool is_a_tty) {
    int print_len = strlen(username) + strlen(nodename) + 1;

    if (is_a_tty) {
        printf(ANSI_BOLD "%s@%s\n" ANSI_RESET, username, nodename);
    } else {
        printf("%s@%s\n", username, nodename);
    }

    for (int i = 0; i < print_len; i++) putchar('-');
    printf("\n");
}

void print_information(const char *label, const char *information, const bool is_a_tty) {
    if (is_a_tty) {
        printf(ANSI_BOLD "%s: " ANSI_RESET, label);
    } else {
        printf("%s: ", label);
    }

    printf("%s\n", information);
}

int get_distro_name(char *dest_buffer, const size_t dest_len) {
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

void format_uptime(long uptime, char *dest_buffer, const size_t dest_len) {
    long days = uptime / 86400;
    uptime %= 86400;
    long hours = uptime / 3600;
    uptime %= 3600;
    long minutes = uptime / 60;
    long seconds = uptime % 60;

    if (days != 0) {
        snprintf(dest_buffer, dest_len, "%ld days, %02ld:%02ld:%02ld", days, hours, minutes, seconds);
    } else {
        snprintf(dest_buffer, dest_len, "%02ld:%02ld:%02ld", hours, minutes, seconds);
    }
}

int get_parent_shell_name(pid_t ppid, char *dest_buffer, const size_t dest_len) {
    char shell_proc_path[BUFFER_SIZE] = {0};
    snprintf(shell_proc_path, sizeof(shell_proc_path), "/proc/%d/comm", ppid);

    FILE *shell_proc_file = fopen(shell_proc_path, "r");
    if (!shell_proc_file) {
        perror("fail to open comm file");
        return -1;
    }

    char buffer[BUFFER_SIZE];
    if (fgets(buffer, BUFFER_SIZE, shell_proc_file) == NULL) {
        fclose(shell_proc_file);
        return -1;
    }
    fclose(shell_proc_file);

    buffer[strcspn(buffer, "\n")] = 0;

    char binary_path[BUFFER_SIZE] = "/usr/bin/";
    strncat(binary_path, buffer, sizeof(binary_path) - strlen(binary_path) - 1);
    
    int pipefd[2]; // [0] - read, [1] - write

    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return 1;
    }

    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork failed");
        return 1;
    } else if (pid == 0) {
        close(pipefd[0]);

        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        execlp(binary_path, buffer, "--version", NULL);

        perror("exec failed");
        exit(1); 
    } else {
        close(pipefd[1]);
        
        FILE *stream = fdopen(pipefd[0], "r");
        if (!stream) {
            perror("fdopen");
            close(pipefd[0]);
            wait(NULL);
            return -1;
        }

        if (fgets(dest_buffer, dest_len, stream) != NULL) {
            size_t len = strlen(dest_buffer);
            if (len > 0 && dest_buffer[len - 1] == '\n') {
                dest_buffer[len - 1] = '\0';
            }
        } else {
            dest_buffer[0] = '\0';
        }

        fclose(stream);
        wait(NULL);
    }

    return 0;
}
