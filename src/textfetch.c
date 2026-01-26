#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cpuid.h>
#include <ctype.h>
#include <dirent.h>
#include <locale.h>
#include <mntent.h>
#include <pwd.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "bitset.h"

#define ANSI_BOLD            "\x1b[1m"
#define ANSI_RESET           "\x1b[0m"
#define BUFFER_SIZE          512
#define BYTES_TO_MIB_DIVISOR 1048576ULL
#define BYTES_TO_GIB_DIVISOR (1024.0 * 1024.0 * 1024.0)

/**
 * Renders the user@hostname header with an underline separator.
 * Colors are applied only if the output is a TTY.
 *
 * @param username Current user's login name.
 * @param nodename System hostname.
 */
void print_header(const char *username, const char *nodename);

/**
 * Prints a labeled system property formatted nicely.
 * Example output: "OS: Debian GNU/Linux"
 *
 * @param label The property name (e.g., "OS", "Kernel").
 * @param information The property value.
 */
void print_information(const char *label, const char *information);

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
 * Retrieves current system RAM and Swap usage statistics from /proc/meminfo.
 * Parses total and available/free memory to calculate used resources, converts 
 * values from kB to MiB, and formats the results as "Used / Total".
 *
 * @param ram_out_buffer Destination buffer to write the formatted RAM string.
 * @param swap_out_buffer Destination buffer to write the formatted Swap string.
 * @param buf_size Size of the destination buffers to prevent overflow.
 */
void get_memory_information(char *ram_out_buffer, char *swap_out_buffer, size_t buf_size);

/**
 * Scans the /sys/class/power_supply/ directory to locate the first device identified as a "Battery".
 *
 * Iterates through directory entries and inspects the "type" file within each subdirectory.
 * If a battery is found, its directory name (e.g., "BAT0") is written to the output buffer.
 * If no battery is detected, writes "No Battery".
 *
 * @param out_buffer Destination buffer to write the detected battery directory name.
 * @param buf_size Size of the destination buffer to prevent overflow.
 */
void find_battery_node(char *out_buffer, size_t buf_size);

/**
 * Safely reads a single-line string from a specified sysfs attribute file.
 *
 * Opens the file, reads its content, and removes the trailing newline character.
 * Uses a zero-initialized internal buffer to prevent reading garbage memory if the file 
 * is empty or unreadable. Writes a hyphen ("-") to the output if the file cannot be opened.
 *
 * @param filepath Full path to the sysfs attribute file (e.g., ".../capacity").
 * @param out_buffer Destination buffer to write the cleaned attribute value.
 * @param buf_size Size of the destination buffer.
 */
void read_battery_attr(const char *filepath, char *out_buffer, size_t buf_size);

/**
 * Aggregates and formats detailed battery information including model, capacity, and status.
 *
 * First attempts to locate a battery node. If found, it constructs paths to specific 
 * attributes (model_name, capacity, status) and formats them into two separate strings:
 * one for the label (e.g., "Battery (Model)") and one for data (e.g., "80% (Discharging)").
 *
 * @param label_out_buffer Buffer to write the descriptive label string.
 * @param information_out_buffer Buffer to write the capacity and charging status.
 * @param buf_size Size of both destination buffers.
 */
void get_battery_information(char *label_out_buffer, char *information_out_buffer, size_t buf_size);

/**
 * Iterates through the system's mounted filesystems to identify and process physical storage devices.
 *
 * This function parses "/proc/mounts" and applies filters to exclude:
 * - Virtual filesystems (checked via "/dev/" prefix).
 * - Loopback devices (e.g., snapd, squashfs).
 * - Specific system partitions (/boot, /var) and duplicate binds (e.g., /home on root).
 *
 * It retrieves filesystem statistics via statvfs() only for valid candidates 
 * to ensure efficient processing before passing data to the display function.
 */
void scan_mounted_volumes();

/**
 * Calculates disk usage statistics and formats the output for display.
 *
 * Converts raw block counts into human-readable GiB format. Computes used space 
 * as (Total - Free) to reflect physical occupancy.
 * Generates a label with the mount point and a formatted string showing usage details 
 * and filesystem type.
 *
 * @param mount_point The directory path where the filesystem is mounted.
 * @param fs_stats    Pointer to the structure containing raw filesystem statistics (blocks).
 * @param mount_entry Pointer to the mntent structure containing filesystem metadata (type, name).
 */
void print_volume_usage(const char *mount_point, const struct statvfs *fs_stats, const struct mntent *mount_entry);

/**
 * Reads a hexadecimal value from a specified sysfs file attribute.
 *
 * Opens the file at the given path, reads the first line, and converts the 
 * hexadecimal string (e.g., "0x10de") into a 16-bit unsigned integer.
 *
 * @param filepath The absolute path to the sysfs attribute file (e.g., ".../vendor").
 * @return The parsed 16-bit ID, or 0 if the file cannot be read, parsed, or the value exceeds 0xFFFF.
 */
uint16_t read_sysfs_hex(const char *filepath);

/**
 * Retrieves and prints human-readable information about a GPU card.
 *
 * Reads the 'vendor' and 'device' IDs from the card's sysfs directory.
 * Translates known Vendor IDs (Nvidia, AMD, Intel) into string names.
 * Formats the output as "Vendor DeviceID" (e.g., "Nvidia 0x1C03") and 
 * passes it to the generic print_information handler.
 *
 * @param card_path The full path to the specific card's directory (e.g., "/sys/class/drm/card0").
 */
void process_gpu_entry(const char *card_path);

/**
 * Scans the DRM subsystem directory (/sys/class/drm/) to find available GPU cards.
 *
 * Iterates through directory entries looking for folders matching the "cardN" pattern,
 * where N is a digit (e.g., "card0", "card1"). Subdirectories not matching this 
 * pattern or containing non-numeric suffixes are skipped.
 * For each valid card found, it triggers the information printing function.
 */
void scan_drm_cards();

bool is_a_tty; 

int main(void) {
    struct utsname machine_info;

    if (uname(&machine_info) != 0) {
        perror("uname");
        return 1;
    }
    
    const char *nodename = machine_info.nodename;
    const char *username = getenv("USER");

    if (!username) username = "unknown";

    is_a_tty = isatty(STDOUT_FILENO);

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
    char swap_information[BUFFER_SIZE] = {0};
    get_memory_information(ram_information, swap_information, sizeof(ram_information));

    char battery_label[BUFFER_SIZE] = {0};
    char battery_information[BUFFER_SIZE] = {0};
    get_battery_information(battery_label, battery_information, BUFFER_SIZE);

    print_header(username, nodename);
    print_information("OS", distro_name);
    print_information("Kernel", machine_info.release);
    print_information("Uptime", uptime_str);
    print_information("Processes", procs_str);
    print_information("Shell", shell_name);
    print_information("Locale", locale);
    print_information("CPU", cpu_information);
    scan_drm_cards();
    print_information("RAM", ram_information);
    print_information("Swap", swap_information);
    scan_mounted_volumes();
    if (strlen(battery_label) != 0) print_information(battery_label, battery_information);

    return 0;
}

void print_header(const char *username, const char *nodename) {
    int print_len = strlen(username) + strlen(nodename) + 1;

    if (is_a_tty) {
        printf(ANSI_BOLD "%s@%s\n" ANSI_RESET, username, nodename);
    } else {
        printf("%s@%s\n", username, nodename);
    }

    for (int i = 0; i < print_len; i++) putchar('-');
    printf("\n");
}

void print_information(const char *label, const char *information) {
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

void get_memory_information(char *ram_out_buffer, char *swap_out_buffer, size_t buf_size) {
    FILE *memory_file = fopen("/proc/meminfo", "r");

    if (!memory_file) {
        perror("failed to open meminfo");
        snprintf(ram_out_buffer, buf_size, "- MiB / - MiB");
        snprintf(swap_out_buffer, buf_size, "- MiB / - MiB");
        return ;
    }

    snprintf(ram_out_buffer, buf_size, "- MiB / - MiB");
    snprintf(swap_out_buffer, buf_size, "0 MiB / 0 MiB");

    char file_line[BUFFER_SIZE];
    uint64_t ram_size = 0;
    uint64_t ram_available = 0;
    uint64_t swap_size = 0;
    uint64_t swap_free = 0;

    while (fgets(file_line, sizeof(file_line), memory_file)) {
        char *delimeter_ptr = strstr(file_line, ": ");
        if (!delimeter_ptr) continue;

        *delimeter_ptr = 0;
        char *key = file_line;
        char *value = delimeter_ptr + 1;
        char *endptr;

        if (strcmp(key, "MemTotal") == 0)     ram_size = strtoull(value, &endptr, 10);
        if (strcmp(key, "MemAvailable") == 0) ram_available = strtoull(value, &endptr, 10);
        if (strcmp(key, "SwapTotal") == 0)    swap_size = strtoull(value, &endptr, 10);
        if (strcmp(key, "SwapFree") == 0)     swap_free = strtoull(value, &endptr, 10);
    }

    fclose(memory_file);

    if (ram_size > 0) {
        uint64_t used_ram = ram_size - ram_available;
        snprintf(ram_out_buffer, buf_size, "%lu MiB / %lu MiB", 
                 used_ram / 1024, ram_size / 1024);
    }

    if (swap_size > 0) {
        uint64_t used_swap = swap_size - swap_free;
        snprintf(swap_out_buffer, buf_size, "%lu MiB / %lu MiB", 
                 used_swap / 1024, swap_size / 1024);
    }
}

void find_battery_node(char *out_buffer, size_t buf_size) {
    struct dirent *entry;
    const char *power_directory = "/sys/class/power_supply/";

    DIR *dir = opendir(power_directory);
    if (dir == NULL) {
        snprintf(out_buffer, buf_size, "No Battery");
        return;
    }

    bool battery_found = false;
    char found_name[256] = {0}; 

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue; 

        char type_path[BUFFER_SIZE];
        snprintf(type_path, sizeof(type_path), "%s%s/type", power_directory, entry->d_name);

        FILE *type_file = fopen(type_path, "r");
        if (!type_file) continue;

        char type_buffer[BUFFER_SIZE] = {0};
        bool is_bat = false;

        if (fgets(type_buffer, sizeof(type_buffer), type_file) != NULL) {
            if (strstr(type_buffer, "Battery")) is_bat = true;
        }

        fclose(type_file);

        if (is_bat) {
            snprintf(found_name, sizeof(found_name), "%s", entry->d_name);
            battery_found = true;
            break;
        }
    }
    closedir(dir);

    if (!battery_found) {
        snprintf(out_buffer, buf_size, "No Battery");
    } else {
        snprintf(out_buffer, buf_size, "%s", found_name);
    }
}

void read_battery_attr(const char *filepath, char *out_buffer, size_t buf_size) {
    char content_buf[BUFFER_SIZE] = {0};
	FILE *sysfs_fp = fopen(filepath, "r");
    if (!sysfs_fp) {
        perror("fail to open sysfs battery file");
        snprintf(out_buffer, buf_size, "-");
        return ;
    }

    fgets(content_buf, sizeof(content_buf), sysfs_fp);

    fclose(sysfs_fp);	

    size_t len = strlen(content_buf);
    if (len > 0 && content_buf[len - 1] == '\n') content_buf[len - 1] = '\0';

    snprintf(out_buffer, buf_size, "%s", content_buf);
}

void get_battery_information(char *label_out_buffer, char *information_out_buffer, size_t buf_size) {
	const char *power_directory = "/sys/class/power_supply/";
    char battery_directory[BUFFER_SIZE];

    find_battery_node(battery_directory, BUFFER_SIZE);

    if (strcmp(battery_directory, "No Battery") == 0) return ;

	char sysfs_attr_path[BUFFER_SIZE + 128];
	char sysfs_attr_buffer[BUFFER_SIZE];

	snprintf(sysfs_attr_path, sizeof(sysfs_attr_path), "%s%s/model_name", power_directory, battery_directory);
    read_battery_attr(sysfs_attr_path, sysfs_attr_buffer, BUFFER_SIZE);
    snprintf(label_out_buffer, buf_size, "Battery (%s)", sysfs_attr_buffer);

	snprintf(sysfs_attr_path, sizeof(sysfs_attr_path), "%s%s/capacity", power_directory, battery_directory);
    read_battery_attr(sysfs_attr_path, sysfs_attr_buffer, BUFFER_SIZE);
    snprintf(information_out_buffer, buf_size, "%s%% ", sysfs_attr_buffer);

	snprintf(sysfs_attr_path, sizeof(sysfs_attr_path), "%s%s/status", power_directory, battery_directory);
    read_battery_attr(sysfs_attr_path, sysfs_attr_buffer, BUFFER_SIZE);
    snprintf(information_out_buffer + strlen(information_out_buffer), buf_size - strlen(information_out_buffer), 
             "(%s)", sysfs_attr_buffer);
}

void scan_mounted_volumes() {
    struct mntent *mnt_entry;
    struct statvfs fs_stats;
    char root_dev_source[BUFFER_SIZE] = "";

    FILE *mounts_file = fopen("/proc/mounts", "r");
    if (!mounts_file) {
        perror("fail to open /proc/mounts");
        return;
    }

    while ((mnt_entry = getmntent(mounts_file)) != NULL) {
        if (strncmp(mnt_entry->mnt_dir, "/run/user/", 10) == 0) continue;

        if (strncmp(mnt_entry->mnt_fsname, "/dev/", 5) != 0) continue; 

        if (strncmp(mnt_entry->mnt_fsname, "/dev/loop", 9) == 0) continue;

        if (strcmp(mnt_entry->mnt_dir, "/boot") == 0 ||
            strcmp(mnt_entry->mnt_dir, "/boot/efi") == 0 ||
            strncmp(mnt_entry->mnt_dir, "/var", 4) == 0) continue;

        if (strncmp(mnt_entry->mnt_dir, "/.", 2) == 0) continue;

        if (strcmp(mnt_entry->mnt_dir, "/") == 0) {
            snprintf(root_dev_source, BUFFER_SIZE, "%s", mnt_entry->mnt_fsname);
        }

        if (strcmp(mnt_entry->mnt_dir, "/home") == 0 && root_dev_source[0] != '\0') {
            if (strcmp(mnt_entry->mnt_fsname, root_dev_source) == 0) {
                continue;
            }
        }

        if (statvfs(mnt_entry->mnt_dir, &fs_stats) != 0) {
            fprintf(stderr, "Warning: statvfs failed for %s\n", mnt_entry->mnt_dir);
            continue;
        }

        if (fs_stats.f_blocks == 0) continue; 

        print_volume_usage(mnt_entry->mnt_dir, &fs_stats, mnt_entry);
    }

    fclose(mounts_file);
}

void print_volume_usage(const char *mount_point, const struct statvfs *fs_stats, const struct mntent *mount_entry) {
    unsigned long long block_size = fs_stats->f_frsize;
    unsigned long long total_size = (unsigned long long)fs_stats->f_blocks * block_size;
    unsigned long long free_size  = (unsigned long long)fs_stats->f_bfree * block_size;
    unsigned long long used_size  = total_size - free_size;

    double total_gb = (double)total_size / BYTES_TO_GIB_DIVISOR;
    double used_gb  = (double)used_size / BYTES_TO_GIB_DIVISOR;

    char label[BUFFER_SIZE];
    snprintf(label, BUFFER_SIZE, "Disk (%s)", mount_point);

    char usage_info[BUFFER_SIZE];
    snprintf(usage_info, BUFFER_SIZE, "%.02f GiB / %.02f GiB (%s)", 
             used_gb, total_gb, mount_entry->mnt_type);

    print_information(label, usage_info);
}

uint16_t read_sysfs_hex(const char *filepath) {
    char content_buf[BUFFER_SIZE] = {0};
    char *endptr;

	FILE *sysfs_fp = fopen(filepath, "r");
    if (!sysfs_fp) {
        perror("fail to open sysfs battery file");
        return 0;
    }

    fgets(content_buf, BUFFER_SIZE, sysfs_fp);

    fclose(sysfs_fp);	

    unsigned long val = strtoul(content_buf, &endptr, 16);

    if (content_buf == endptr || val > 0xFFFF) return 0;

    return (uint16_t)val;
}

void process_gpu_entry(const char *card_path) {
    char sysfs_attr_path[BUFFER_SIZE] = "";
    char output_buf[BUFFER_SIZE] = "";

    snprintf(sysfs_attr_path, BUFFER_SIZE, "%s/device/vendor", card_path);
    uint16_t vendor_id = read_sysfs_hex(sysfs_attr_path);

    snprintf(sysfs_attr_path, BUFFER_SIZE, "%s/device/device", card_path);
    uint16_t device_id = read_sysfs_hex(sysfs_attr_path);

    const char *vendor_name = "Unknown";
    char hex_vendor[16];

    switch (vendor_id) {
        case 0x1002: vendor_name = "AMD"; break;
        case 0x10de: vendor_name = "Nvidia"; break;
        case 0x8086: vendor_name = "Intel"; break;
        default:
            snprintf(hex_vendor, sizeof(hex_vendor), "0x%04X", vendor_id);
            vendor_name = hex_vendor;
            break;
    }

    snprintf(output_buf, BUFFER_SIZE, "%s 0x%04X", vendor_name, device_id);

    print_information("GPU", output_buf);
}

void scan_drm_cards() {
    struct dirent *dir_entry;
    const char *gpu_directory = "/sys/class/drm/";
    const char *prefix = "card";
    size_t prefix_len = 4;

    DIR *dir_handle = opendir(gpu_directory);
    if (!dir_handle) {
        fprintf(stderr, "Warning: direct failed for %s\n", gpu_directory);
        return ;
    }

    char card_path[BUFFER_SIZE] = "";
    while ((dir_entry = readdir(dir_handle)) != NULL) {
        if (strncmp(dir_entry->d_name, prefix, prefix_len) != 0) continue;

        const char *suffix = dir_entry->d_name + prefix_len;

        if (*suffix == '\0') continue;

        const char *cursor = suffix;
        int valid_name = 1;
        while (*cursor) {
            if (!isdigit(*cursor)) {
                valid_name = 0;
                break;
            }

            cursor++;
        }

        if (!valid_name) continue;

        snprintf(card_path, BUFFER_SIZE, "%s%s", gpu_directory, dir_entry->d_name);
        process_gpu_entry(card_path);
    }

    closedir(dir_handle);
}
