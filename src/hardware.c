/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__x86_64__) || defined(__i386__)
    #include <cpuid.h>
#endif

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
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
#include "hardware.h"
#include "ui.h"

/**
 * CPU Information Helpers
 * Functions to query CPUID for model name, scan sysfs for physical cores,
 * and read current scaling frequency.
 */

/**
 * Aggregates CPU model, core count, and current frequency into a single string.
 * 
 * @param out_buf  The destination buffer.
 * @param buf_size The size of the destination buffer.
 */
static void hw_get_cpu_info(char *out_buf, const size_t buf_size);

/**
 * Retrieves the CPU brand string.
 * On x86/i386, it uses the CPUID instruction. On other architectures, 
 * it falls back to parsing /proc/cpuinfo for the 'model name' field.
 * 
 * @param model_buf The buffer to store the model name.
 */
static void hw_get_cpu_model(char *model_buf);

/**
 * Calculates the number of unique physical CPU cores using sysfs topology.
 * Uses a bitset to filter out logical threads (Hyper-threading).
 * 
 * @param core_count Pointer to store the result.
 */
static void hw_get_cpu_cores(uint32_t *core_count);

/**
 * Reads the current CPU scaling frequency from sysfs.
 * Converts KHz to GHz and fractional parts.
 * 
 * @param out_ghz   Pointer to store the GHz integer.
 * @param out_frac  Pointer to store the fractional part (3 decimal places).
 */
static void hw_get_cpu_freq(uint32_t *out_ghz, uint32_t *out_frac);

/**
 * GPU Detection Helpers
 * Functions to scan the DRM subsystem and translate PCI vendor/device IDs.
 */

/**
 * Scans /sys/class/drm/ for directories matching the 'cardN' pattern.
 * Triggers processing for each valid GPU found.
 */
static void hw_scan_gpus(void);

/**
 * Reads vendor and device IDs for a specific GPU card.
 * Translates known PCI IDs (Intel, AMD, Nvidia) and prints the result.
 * 
 * @param card_path The sysfs path to the card directory.
 */
static void hw_process_gpu(const char *card_path);

/**
 * Storage and Memory Helpers
 * Functions to parse /proc/meminfo and /proc/mounts for resource usage.
 */

/**
 * Parses /proc/meminfo to calculate used/total RAM and Swap.
 * Formats results as "Used MiB / Total MiB".
 * 
 * @param ram_buf  Buffer for RAM information.
 * @param swap_buf Buffer for Swap information.
 * @param buf_size Size of the buffers.
 */
static void hw_get_mem_info(char *ram_buf, char *swap_buf, const size_t buf_size);

/**
 * Parses /proc/mounts to find physical storage devices.
 * Filters out virtual and system partitions to find user-relevant disks.
 */
static void hw_scan_disks(void);

/**
 * Calculates disk usage statistics and prints the result via the UI module.
 * 
 * @param mnt The mount point path.
 * @param fs  The filesystem statistics structure.
 * @param ent The mount entry metadata.
 */
static void hw_print_disk(const char *mnt, const struct statvfs *fs, const struct mntent *ent);

/**
 * Power Supply Helpers
 * Functions to locate the primary battery and read its model, capacity, and status.
 */

/**
 * Collects battery model, capacity, and charging status.
 * 
 * @param label_buf Buffer for the "Battery (Model)" label.
 * @param info_buf  Buffer for the "Capacity% (Status)" string.
 * @param buf_size  Size of the buffers.
 */
static void hw_get_bat_info(char *label_buf, char *info_buf, const size_t buf_size);

/**
 * Searches /sys/class/power_supply/ for the first device of type 'Battery'.
 * 
 * @param out_buf  Buffer to store the directory name (e.g., BAT0).
 * @param buf_size Size of the buffer.
 */
static void hw_find_battery(char *out_buf, const size_t buf_size);

/**
 * Internal Utility Helpers
 * Low-level functions to read hexadecimal and string attributes from sysfs files.
 */

/**
 * Reads a 16-bit hexadecimal value from a sysfs attribute file.
 * 
 * @param path Path to the sysfs file.
 * @return The parsed 16-bit value, or 0 on failure.
 */
static uint16_t hw_read_hex(const char *path);

/**
 * Reads a single-line string attribute from sysfs and removes the newline.
 * 
 * @param path     Path to the sysfs file.
 * @param out_buf  The destination buffer.
 * @param buf_size Size of the destination buffer.
 */
static void hw_read_attr(const char *path, char *out_buf, const size_t buf_size);

void hardware_print_info(void) {
    char cpu_buf[LINE_BUFFER] = {0};
    hw_get_cpu_info(cpu_buf, LINE_BUFFER);
    ui_print_info("CPU", cpu_buf);

    hw_scan_gpus();

    char ram_buf[LINE_BUFFER] = {0};
    char swap_buf[LINE_BUFFER] = {0};
    hw_get_mem_info(ram_buf, swap_buf, LINE_BUFFER);
    ui_print_info("RAM", ram_buf);
    ui_print_info("Swap", swap_buf);

    hw_scan_disks();

    char battery_label[LINE_BUFFER] = {0};
    char battery_buf[LINE_BUFFER] = {0};
    hw_get_bat_info(battery_label, battery_buf, LINE_BUFFER);

    if (strlen(battery_label) != 0) ui_print_info(battery_label, battery_buf);
}

static void hw_get_cpu_info(char *out_buf, const size_t buf_size) {
    uint32_t phy_core_count = 0;
    uint32_t frequency_ghz = 0;
    uint32_t freq_fractional = 0;
    char cpu_model_name[LINE_BUFFER] = {0};

    hw_get_cpu_model(cpu_model_name);
    hw_get_cpu_cores(&phy_core_count);
    hw_get_cpu_freq(&frequency_ghz, &freq_fractional);
 
    snprintf(out_buf, buf_size, "%s (%u) @ %u.%03u GHz", 
             cpu_model_name, phy_core_count, frequency_ghz, freq_fractional);
}

#if defined(__x86_64__) || defined(__i386__)
static void hw_get_cpu_model(char *model_buf) {
    unsigned int regs[4];
    char *write_cursor = model_buf;

    for (unsigned int leaf_id = 0x80000002; leaf_id <= 0x80000004; leaf_id++) {
        if (__get_cpuid(leaf_id, &regs[0], &regs[1], &regs[2], &regs[3]) == 0) {
            return ;
        }

        memcpy(write_cursor, regs, sizeof(regs));
        write_cursor += sizeof(regs);
    }

    *write_cursor = '\0';

    size_t len = strlen(model_buf);

    while (len > 0 && isspace((unsigned char)model_buf[len - 1])) {
        len--;
        model_buf[len] = '\0';
    }

    return ;
}
#else
static void hw_get_cpu_model(char *model_buf) {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        fprintf(stderr, "Error: failed to open CPU info file: %s\n", strerror(errno));
        return ;
    }

    char line[LINE_BUFFER];
    while (fgets(line, LINE_BUFFER, fp)) {
        char *delim = strchr(line, ':');
        if (!delim) continue;

        *delim = 0;
        char *key = line;
        char *value = delim + 1;

        value[strcspn(value, "\n")] = 0;

        if (value[0] == ' ') value++;

        if (strncmp(key, "model name", 10) == 0) {
            snprintf(model_buf, LINE_BUFFER, "%s", value);
            break;
        }
    }

    fclose(fp);
}
#endif

static void hw_get_cpu_cores(uint32_t *core_count) {
    struct stat sb;
    memset(&sb, 0, sizeof(struct stat));

    uint32_t unique_cores[SET_SIZE] = {0};
    int max_logical_cpus = sysconf(_SC_NPROCESSORS_CONF);

    char sysfs_path_buf[LINE_BUFFER] = "/sys/devices/system/cpu/";
    size_t len = strlen(sysfs_path_buf);
    char *path_cursor = sysfs_path_buf + len;
    ssize_t buffer_capacity = LINE_BUFFER - len;

    for (int i = 0; i < max_logical_cpus; i++) {
        snprintf(path_cursor, buffer_capacity, "cpu%d/", i);

        if (stat(sysfs_path_buf, &sb) != 0 || !S_ISDIR(sb.st_mode)) break;

        char core_path[PATH_MAX] = {0};
        snprintf(core_path, sizeof(core_path), "%stopology/core_id", sysfs_path_buf);

        int core_id = -1;
        FILE *core_file = fopen(core_path, "r");
        if (!core_file) {
            fprintf(stderr, "Error: failed to open CPU core ID file: %s\n", strerror(errno));
            break;
        }

        if (fscanf(core_file, "%d", &core_id) != 1) {
            fprintf(stderr, "Error: failed to parse numeric value from sysfs\n");
        }

        fclose(core_file);
        
        if (!set_contains(unique_cores, core_id)) set_add(unique_cores, core_id);
    }

    *core_count = count_set_bits(unique_cores, SET_SIZE);
}

static void hw_get_cpu_freq(uint32_t *out_ghz, uint32_t *out_frac) {
    FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (!fp) {
        fprintf(stderr, "Error: failed to open CPU frequency file: %s\n", strerror(errno));
        return ;
    } 

    uint32_t frequency_khz = 0;
    if (fscanf(fp, "%u", &frequency_khz) == -1) {
        fprintf(stderr, "Error: failed to parse numeric value from sysfs\n");
    }

    fclose(fp);

    if (frequency_khz == 0) {
        *out_ghz = 0;
        *out_frac = 0;
        return ;
    }

    *out_ghz = frequency_khz / 1000000;
    *out_frac = (frequency_khz % 1000000) / 1000;
}

static void hw_scan_gpus(void) {
    struct dirent *dir_entry;
    const char *gpu_directory = "/sys/class/drm/";
    const char *prefix = "card";
    size_t prefix_len = sizeof("card") - 1;

    DIR *dir_handle = opendir(gpu_directory);
    if (!dir_handle) {
        fprintf(stderr, "Error: failed to open GPU directory %s: %s\n", 
                gpu_directory, strerror(errno));
        return ;
    }

    char card_path[PATH_MAX] = "";
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

        snprintf(card_path, PATH_MAX, "%s%s", gpu_directory, dir_entry->d_name);
        hw_process_gpu(card_path);
    }

    closedir(dir_handle);
}

static void hw_process_gpu(const char *card_path) {
    char sysfs_attr_path[PATH_MAX] = "";
    char output_buf[LINE_BUFFER] = "";

    snprintf(sysfs_attr_path, PATH_MAX, "%s/device/vendor", card_path);
    uint16_t vendor_id = hw_read_hex(sysfs_attr_path);

    snprintf(sysfs_attr_path, PATH_MAX, "%s/device/device", card_path);
    uint16_t device_id = hw_read_hex(sysfs_attr_path);

    const char *vendor_name = "Unknown";
    char hex_vendor[16];

    switch (vendor_id) {
        case 0x1002: vendor_name = "AMD"; break;
        case 0x106b: vendor_name = "Apple"; break;
        case 0x10de: vendor_name = "Nvidia"; break;
        case 0x8086: vendor_name = "Intel"; break;
        default:
            snprintf(hex_vendor, sizeof(hex_vendor), "0x%04X", vendor_id);
            vendor_name = hex_vendor;
            break;
    }

    snprintf(output_buf, LINE_BUFFER, "%s 0x%04X", vendor_name, device_id);

    ui_print_info("GPU", output_buf);
}

static void hw_get_mem_info(char *ram_buf, char *swap_buf, const size_t buf_size) {
    FILE *memory_file = fopen("/proc/meminfo", "r");
    if (!memory_file) {
        fprintf(stderr, "Error: failed to open /proc/meminfo: %s\n", strerror(errno));
        snprintf(ram_buf, buf_size, "- MiB / - MiB");
        snprintf(swap_buf, buf_size, "- MiB / - MiB");
        return ;
    }

    snprintf(ram_buf, buf_size, "- MiB / - MiB");
    snprintf(swap_buf, buf_size, "0 MiB / 0 MiB");

    char file_line[LINE_BUFFER];
    uint64_t ram_size = 0;
    uint64_t ram_available = 0;
    uint64_t swap_size = 0;
    uint64_t swap_free = 0;

    while (fgets(file_line, LINE_BUFFER, memory_file)) {
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
        snprintf(ram_buf, buf_size, "%llu MiB / %llu MiB", 
                 (unsigned long long)(used_ram / 1024), 
                 (unsigned long long)(ram_size / 1024));
    }

    if (swap_size > 0) {
        uint64_t used_swap = swap_size - swap_free;
        snprintf(swap_buf, buf_size, "%llu MiB / %llu MiB", 
                 (unsigned long long)(used_swap / 1024), 
                 (unsigned long long)(swap_size / 1024));
    }
}

static void hw_scan_disks(void) {
    struct mntent *mnt_entry;
    struct statvfs fs;
    memset(&fs, 0, sizeof(struct statvfs));

    char root_dev_source[PATH_MAX] = "";

    FILE *mounts_file = fopen("/proc/mounts", "r");
    if (!mounts_file) {
        fprintf(stderr, "Error: failed to open /proc/mounts: %s\n", strerror(errno));
        return;
    }

    while ((mnt_entry = getmntent(mounts_file)) != NULL) {
        if (strncmp(mnt_entry->mnt_fsname, "/dev/", 5) != 0) continue; 

        if (strncmp(mnt_entry->mnt_fsname, "/dev/loop", 9) == 0) continue;

        if (strncmp(mnt_entry->mnt_dir, "/.", 2) == 0 ||
            strcmp(mnt_entry->mnt_dir, "/boot") == 0 ||
            strcmp(mnt_entry->mnt_dir, "/boot/efi") == 0 ||
            strcmp(mnt_entry->mnt_dir, "/mnt/wslg/distro") == 0 ||
            strncmp(mnt_entry->mnt_dir, "/run/user/", 10) == 0 ||
            strncmp(mnt_entry->mnt_dir, "/var", 4) == 0) continue;

        if (strcmp(mnt_entry->mnt_dir, "/") == 0) {
            snprintf(root_dev_source, PATH_MAX, "%s", mnt_entry->mnt_fsname);
        }

        if (strcmp(mnt_entry->mnt_dir, "/home") == 0 && root_dev_source[0] != '\0') {
            if (strcmp(mnt_entry->mnt_fsname, root_dev_source) == 0) {
                continue;
            }
        }

        if (statvfs(mnt_entry->mnt_dir, &fs) != 0) {
            fprintf(stderr, "Error: statvfs failed for %s: %s\n", 
                    mnt_entry->mnt_dir, strerror(errno));
            continue;
        }

        if (fs.f_blocks == 0) continue; 

        hw_print_disk(mnt_entry->mnt_dir, &fs, mnt_entry);
    }

    fclose(mounts_file);
}

static void hw_print_disk(const char *mnt, const struct statvfs *fs, const struct mntent *ent) {
    unsigned long long block_size = fs->f_frsize;
    unsigned long long total_size = (unsigned long long)fs->f_blocks * block_size;
    unsigned long long free_size  = (unsigned long long)fs->f_bfree * block_size;
    unsigned long long used_size  = total_size - free_size;

    double total_gb = (double)total_size / BYTES_TO_GIB_DIVISOR;
    double used_gb  = (double)used_size / BYTES_TO_GIB_DIVISOR;

    char label[LINE_BUFFER];
    snprintf(label, LINE_BUFFER, "Disk (%s)", mnt);

    char usage_info[LINE_BUFFER];
    snprintf(usage_info, LINE_BUFFER, "%.02f GiB / %.02f GiB (%s)", 
             used_gb, total_gb, ent->mnt_type);

    ui_print_info(label, usage_info);
}

static void hw_get_bat_info(char *label_buf, char *info_buf, const size_t buf_size) {
	const char *power_directory = "/sys/class/power_supply/";
    char bat_dir[SMALL_BUFFER];

    hw_find_battery(bat_dir, SMALL_BUFFER);

    if (strcmp(bat_dir, "No Battery") == 0) return ;

	char sysfs_attr_path[PATH_MAX];
	char attr_buf[SMALL_BUFFER];

	snprintf(sysfs_attr_path, sizeof(sysfs_attr_path), "%s%s/model_name", power_directory, bat_dir);
    hw_read_attr(sysfs_attr_path, attr_buf, SMALL_BUFFER);
    snprintf(label_buf, buf_size, "Battery (%s)", attr_buf);

	snprintf(sysfs_attr_path, sizeof(sysfs_attr_path), "%s%s/capacity", power_directory, bat_dir);
    hw_read_attr(sysfs_attr_path, attr_buf, SMALL_BUFFER);
    snprintf(info_buf, buf_size, "%s%% ", attr_buf);

	snprintf(sysfs_attr_path, sizeof(sysfs_attr_path), "%s%s/status", power_directory, bat_dir);
    hw_read_attr(sysfs_attr_path, attr_buf, SMALL_BUFFER);
    snprintf(info_buf + strlen(info_buf), buf_size - strlen(info_buf), 
             "(%s)", attr_buf);
}

static void hw_find_battery(char *out_buf, const size_t buf_size) {
    snprintf(out_buf, buf_size, "No Battery");

    const char *power_path = "/sys/class/power_supply/";
    DIR *dir = opendir(power_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue; 

        char type_path[PATH_MAX];
        snprintf(type_path, sizeof(type_path), "%s%s/type", power_path, entry->d_name);

        FILE *fp = fopen(type_path, "r");
        if (!fp) continue;

        char buf[SMALL_BUFFER];
        if (fgets(buf, SMALL_BUFFER, fp) && strncmp(buf, "Battery", 7) == 0) {
            snprintf(out_buf, buf_size, "%.*s", (int)(buf_size - 1), entry->d_name);
            fclose(fp);
            break;
        }
        fclose(fp);
    }

    closedir(dir);
}

static uint16_t hw_read_hex(const char *path) {
    char content_buf[LINE_BUFFER] = {0};
    char *endptr;

	FILE *sysfs_fp = fopen(path, "r");
    if (!sysfs_fp) {
        fprintf(stderr, "Error: failed to open sysfs file: %s\n", strerror(errno));
        return 0;
    }

    fgets(content_buf, LINE_BUFFER, sysfs_fp);

    fclose(sysfs_fp);	

    unsigned long val = strtoul(content_buf, &endptr, 16);

    if (content_buf == endptr || val > 0xFFFF) return 0;

    return (uint16_t)val;
}

static void hw_read_attr(const char *path, char *out_buf, const size_t buf_size) {
    char content_buf[LINE_BUFFER] = {0};

	FILE *sysfs_fp = fopen(path, "r");
    if (!sysfs_fp) {
        fprintf(stderr, "Error: failed to open sysfs file: %s\n", strerror(errno));
        snprintf(out_buf, buf_size, "-");
        return ;
    }

    fgets(content_buf, LINE_BUFFER, sysfs_fp);

    fclose(sysfs_fp);	

    size_t len = strlen(content_buf);
    if (len > 0 && content_buf[len - 1] == '\n') content_buf[len - 1] = '\0';

    snprintf(out_buf, buf_size, "%s", content_buf);
}
