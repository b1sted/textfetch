#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cpuid.h>
#include <ctype.h>
#include <locale.h>
#include <pwd.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "bitset.h"

#define ANSI_BOLD             "\x1b[1m"
#define ANSI_RESET            "\x1b[0m"
#define BUFFER_SIZE           512
#define BYTES_TO_MIB_DIVISOR  1048576ULL

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

/**
 * Retrieves the CPU brand string using CPUID instructions.
 * Queries extended CPUID functions to construct the raw model name 
 * and removes any trailing whitespace from the result.
 *
 * @param out_model_buf Destination buffer to store the model name string.
 * @return 0 on success, -1 if CPUID is not supported or fails.
 */
int get_cpu_model_name(char *out_model_buf);

/**
 * Calculates the number of unique physical CPU cores.
 * Iterates through the system topology via sysfs (cpuN/topology/core_id)
 * to distinguish between physical cores and logical threads.
 *
 * @param out_physical_count Pointer to store the calculated number of physical cores.
 */
void get_cpu_cores_number(uint32_t *out_physical_count);

/**
 * Retrieves the current CPU frequency from sysfs.
 * Reads the scaling_cur_freq file (in KHz) and converts it into 
 * separate GHz integer and fractional parts for formatted output.
 *
 * @param out_ghz Pointer to store the integer part (Gigahertz).
 * @param out_fractional Pointer to store the fractional part (3 decimal places).
 */
void get_cpu_current_frequency(uint32_t *out_ghz, uint32_t *out_fractional);

/**
 * Aggregates all CPU information into a single formatted string.
 * Combines model name, physical core count, and current frequency
 * into a human-readable format.
 *
 * @param out_buffer Destination buffer to write the formatted string.
 * @param buf_size Size of the destination buffer to prevent overflow.
 */
void get_cpu_information(char *out_buffer, size_t buf_size);

/**
 * Retrieves current system RAM usage statistics from /proc/meminfo.
 * Parses total and available memory to calculate used RAM, converts 
 * values from kB to MB, and formats the result as "Used / Total".
 *
 * @param out_buffer Destination buffer to write the formatted string.
 * @param buf_size Size of the destination buffer to prevent overflow.
 */
void get_ram_information(char *out_buffer, size_t buf_size);

int main(void) {
    struct utsname machine_info;

    if (uname(&machine_info) != 0) {
        perror("uname");
        return 1;
    }
    
    const char *nodename = machine_info.nodename;
    const char *username = getenv("USER");

    if (!username) username = "unknown";

    const bool is_a_tty = isatty(STDOUT_FILENO);

    char distro_name[BUFFER_SIZE] = {0};

    if (get_distro_name(distro_name, BUFFER_SIZE) != 0) {
        fprintf(stderr, "Failed to read os-release\n");
        snprintf(distro_name, sizeof(distro_name), "%s", machine_info.sysname);
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
    if (!locale) locale = "-";

    char cpu_information[BUFFER_SIZE] = {0};
    get_cpu_information(cpu_information, sizeof(cpu_information));

    char ram_information[BUFFER_SIZE] = {0};
    get_ram_information(ram_information, sizeof(ram_information));

    print_header(username, nodename, is_a_tty);
    print_information("OS", distro_name, is_a_tty);
    print_information("Kernel", machine_info.release, is_a_tty);
    print_information("Uptime", uptime_str, is_a_tty);
    print_information("Processes", procs_str, is_a_tty);
    print_information("Shell", shell_name, is_a_tty);
    print_information("Locale", locale, is_a_tty);
    print_information("CPU", cpu_information, is_a_tty);
    print_information("RAM", ram_information, is_a_tty);

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
            snprintf(dest_buffer, dest_len, "%s", buffer);
        }

        fclose(stream);
        wait(NULL);
    }

    return 0;
}

int get_cpu_model_name(char *out_model_buf) {
    unsigned int cpuid_regs[4];
    char *write_cursor = out_model_buf;

    for (unsigned int leaf_id = 0x80000002; leaf_id <= 0x80000004; leaf_id++) {
        if (__get_cpuid(leaf_id, &cpuid_regs[0], &cpuid_regs[1], &cpuid_regs[2], &cpuid_regs[3]) == 0) {
            return -1;
        }

        memcpy(write_cursor, cpuid_regs, sizeof(cpuid_regs));
        write_cursor += sizeof(cpuid_regs);
    }

    *write_cursor = '\0';

    size_t len = strlen(out_model_buf);

    while (len > 0 && isspace((unsigned char)out_model_buf[len - 1])) {
        len--;
        out_model_buf[len] = '\0';
    }

    return 0;
}

void get_cpu_cores_number(uint32_t *out_physical_count) {
    uint32_t unique_cores[SET_SIZE] = {0};
    int max_logical_cpus = sysconf(_SC_NPROCESSORS_CONF);

    struct stat sb;
    char sysfs_path_buf[BUFFER_SIZE] = "/sys/devices/system/cpu/";
    char *path_cursor = sysfs_path_buf + strlen(sysfs_path_buf);
    ssize_t buffer_capacity = BUFFER_SIZE - strlen(sysfs_path_buf);

    for (int i = 0; i < max_logical_cpus; i++) {
        snprintf(path_cursor, buffer_capacity, "cpu%d/", i);

        if (stat(sysfs_path_buf, &sb) != 0 || !S_ISDIR(sb.st_mode)) break;

        char core_path[BUFFER_SIZE + 32];
        snprintf(core_path, sizeof(core_path), "%stopology/core_id", sysfs_path_buf);

        int core_id = -1;
        FILE *core_file = fopen(core_path, "r");

        if (!core_file) {
            perror("fail to open cpu core id file");
            break;
        }

        if (fscanf(core_file, "%d", &core_id) != 1) printf("Ошибка чтения числа");

        fclose(core_file);
        
        if (!set_contains(unique_cores, core_id)) set_add(unique_cores, core_id);
    }

    *out_physical_count = count_set_bits(unique_cores, SET_SIZE);
}

void get_cpu_current_frequency(uint32_t *out_ghz, uint32_t *out_fractional) {
    FILE *frequency_file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");

    if (!frequency_file) {
        perror("failed to open scaling_cur_freq");
        return ;
    } 

    uint32_t frequency_khz = 0;
    if (fscanf(frequency_file, "%u", &frequency_khz) == -1) {
        printf("Ошибка чтения числа");
    }

    fclose(frequency_file);

    if (frequency_khz == 0) {
        *out_ghz = 0;
        *out_fractional = 0;
        return ;
    }

    *out_ghz = frequency_khz / 1000000;
    *out_fractional = (frequency_khz % 1000000) / 1000;
}

void get_cpu_information(char *out_buffer, size_t buf_size) {
    uint32_t phy_core_count = 0;
    uint32_t frequency_ghz = 0;
    uint32_t freq_fractional = 0;
    char cpu_model_name[BUFFER_SIZE] = {0};

    if (get_cpu_model_name(cpu_model_name) != 0) {
        snprintf(cpu_model_name, BUFFER_SIZE, "%s", "unkwown");
    }

    get_cpu_cores_number(&phy_core_count);
    
    get_cpu_current_frequency(&frequency_ghz, &freq_fractional);
 
    snprintf(out_buffer, buf_size, "%s (%u) @ %u.%03u GHz", 
             cpu_model_name, phy_core_count, frequency_ghz, freq_fractional);
}

void get_ram_information(char *out_buffer, size_t buf_size) {
    FILE *memory_file = fopen("/proc/meminfo", "r");

    if (!memory_file) {
        perror("failed to open meminfo");
        snprintf(out_buffer, buf_size, "- MiB / - MiB");
        return ;
    }

    char file_line[BUFFER_SIZE];
    uint64_t ram_size = 0;
    uint64_t ram_available = 0;

    while (fgets(file_line, sizeof(file_line), memory_file)) {
        char *delimeter_ptr = strstr(file_line, ": ");
        if (!delimeter_ptr) continue;

        *delimeter_ptr = 0;
        char *key = file_line;
        char *value = delimeter_ptr + 1;

        value[strcspn(value, "\n")] = 0;
        char *endptr;

        if (strcmp(key, "MemTotal") == 0) {
            ram_size = strtoull(value, &endptr, 10);
        }

        if (strcmp(key, "MemAvailable") == 0) {
            ram_available = strtoull(value, &endptr, 10);
            break;
        }
    }

    fclose(memory_file);

    if (ram_available == 0 || ram_size == 0) {
        snprintf(out_buffer, buf_size, "- MiB / %lu MiB", ram_size / 1024);
        return ;
    }

    uint64_t used_ram = ram_size - ram_available;
    
    used_ram /= 1024;
    ram_size /= 1024;

    snprintf(out_buffer, buf_size, "%lu MiB / %lu MiB", used_ram, ram_size);
}
