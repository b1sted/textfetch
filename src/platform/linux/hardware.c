/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include "pal/hardware_os.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <ctype.h>
#include <errno.h>

#include <dirent.h>
#include <inttypes.h>
#include <limits.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif

#if defined(__arm__) || defined(__aarch64__)
#include "arm_ids.h"
#endif

#include "binary_trees.h"
#include "bitset.h"
#include "defs.h"
#include "hashtable.h"
#include "sys_utils.h"
#include "ui.h"

/* Array of paths to search for the pci.ids database file. */
#define PCI_IDS_PATHS                                                                              \
    {                                                                                              \
        "/var/lib/pciutils/pci.ids", "/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids",       \
            "/usr/share/pci.ids", "/opt/homebrew/share/pci.ids", "pci.ids", NULL                   \
    }

/* Base directories in sysfs and procfs for hardware enumeration. */
#define SYS_CPU_DIR "/sys/devices/system/cpu/"
#define CPUINFO_PATH "/proc/cpuinfo"
#define SYS_GPU_DIR "/sys/class/drm/"
#define SYS_BATTERY_DIR "/sys/class/power_supply/"

/* Maximum supported limits for CPU discovery. */
#define MAX_CPU_PACKAGES 16
#define MAX_CPU_CORES 256

/* Number of blocks needed for the CPU cores bitset. */
#define CORES_SET_BLOCKS SET_SIZE(MAX_CPU_CORES)

/* Maximum number of ranges kept from the sysfs online CPU map. */
#define MAX_ONLINE_RANGES 64

/* Estimated capacity for the embedded ARM ID forest. */
#define ARM_IMPLEMENTERS 25

/* cpufreq attributes probed for a core, ordered from best to fallback. */
static const char *const CPU_FREQ_FILES[] = {"cpuinfo_max_freq", "scaling_max_freq", "bios_limit",
                                             "scaling_cur_freq", "cpuinfo_cur_freq", NULL};

/* Internal structure to store CPU parsing progress per socket. */
typedef struct {
    char model[MEDIUM_BUFFER];
    double frequency;
    uint32_t core_bits[CORES_SET_BLOCKS];
    bool exists;
} cpu_data_t;

/* Inclusive range of logical CPU IDs from the sysfs online map. */
typedef struct {
    uint32_t first;
    uint32_t last;
} cpu_range_t;

/* Parsed /sys/devices/system/cpu/online file. */
typedef struct {
    cpu_range_t ranges[MAX_ONLINE_RANGES];
    size_t count;
    bool valid;
} cpu_online_map_t;

/* Generic linked list node for hardware enumeration. */
typedef struct hw_node {
    char name[SMALL_BUFFER];
    struct hw_node *next;
} hw_node_t;

/* Linked list node representing a raw PCI GPU device format. */
typedef struct gpu_node {
    uint16_t impl_id;
    uint16_t part_id;
    struct gpu_node *next;
} gpu_node_t;

/* Internal structure for grouping similar GPU models. */
typedef struct {
    char model[LINE_BUFFER];
} gpu_data_t;

/* Internal structure to store parsed battery metrics. */
typedef struct {
    char model[SMALL_BUFFER];
    char capacity[TINY_BUFFER];
    char status[MINI_BUFFER];
    uint8_t health;
} bat_data_t;

/* Bitmask flags representing the calculated health status of a battery. */
typedef enum {
    HEALTH_GOOD = 1 << 0,
    HEALTH_FAIR = 1 << 1,
    HEALTH_POOR = 1 << 2,
    HEALTH_UNKNOWN = 1 << 3
} battery_health_flags_t;

/**
 * Parses the sysfs CPU directory in a single pass to construct the CPU topology.
 *
 * Every core is read exactly once: online state, package ID, core ID, and
 * frequency. No nested directory rescans are performed.
 *
 * @param cpu_dir Base sysfs CPU directory to scan.
 * @param cpus Array of CPU data structures to populate.
 * @param cpu_map Array mapping logical CPU IDs to physical sockets.
 * @param packages Pointer to store the total number of CPU packages found.
 */
static void hw_parse_cpu_topology(const char *cpu_dir, cpu_data_t *cpus, uint16_t *cpu_map,
                                  uint8_t *packages);

/**
 * Parses the sysfs online CPU map into memory.
 *
 * A single read of /sys/devices/system/cpu/online replaces the per-CPU online
 * attribute probes. The map is marked invalid when the file is missing or
 * malformed, in which case callers fall back to the per-CPU attribute.
 *
 * @param cpu_dir Base sysfs CPU directory containing the online map.
 * @param map Destination for the parsed inclusive CPU ranges.
 */
static void hw_cpu_read_online_map(const char *cpu_dir, cpu_online_map_t *map);

/**
 * Checks whether the sysfs cpufreq interface is available.
 *
 * @param cpu_dir Base sysfs CPU directory to inspect.
 * @return true when at least one cpufreq policy directory exists.
 */
static bool hw_cpu_has_cpufreq(const char *cpu_dir);

/**
 * Parses hardware properties of a single logical CPU core node.
 *
 * @param cpu_dir Base sysfs CPU directory containing the node.
 * @param node_name The directory name of the CPU node (e.g., "cpu0").
 * @param cpus Array of CPU data structures to be updated.
 * @param cpu_map Array mapping logical CPU IDs to physical sockets.
 * @param packages Pointer to the total number of CPU packages found.
 * @param online_map Parsed online CPU map, or NULL when unavailable.
 * @param freq_available Whether cpufreq attributes may be probed.
 */
static void hw_cpu_process_core_node(const char *cpu_dir, const char *node_name, cpu_data_t *cpus,
                                     uint16_t *cpu_map, uint8_t *packages,
                                     const cpu_online_map_t *online_map, bool freq_available);

/**
 * Probes one CPU and raises the package peak frequency if it is faster.
 *
 * @param cpu_dir Base sysfs CPU directory to scan.
 * @param cpus Array of CPU data structures.
 * @param package The physical package ID.
 * @param cpu_name The string identifying the CPU node in sysfs.
 */
static void hw_get_cpu_freq(const char *cpu_dir, cpu_data_t *cpus, const int16_t package,
                            const char *cpu_name);

/**
 * Checks whether every discovered CPU package already has a model name.
 *
 * This lets the /proc/cpuinfo parser stop early instead of reading the whole
 * file on homogeneous systems.
 *
 * @param cpus Array of CPU data structures.
 * @return true if at least one package exists and all of them have a model.
 */
static bool hw_cpu_models_complete(const cpu_data_t *cpus);

/**
 * Checks whether a logical CPU is currently online.
 *
 * The parsed online map is authoritative when available. A missing per-CPU
 * online attribute means the CPU cannot be hotplugged and is therefore always
 * online, as is the case for CPU 0 on all Linux kernels.
 *
 * @param cpu_dir Base sysfs CPU directory containing the node.
 * @param online_map Parsed online CPU map, or NULL when unavailable.
 * @param node_name The directory name of the CPU node (e.g., "cpu0").
 * @return true if the CPU is online or its state cannot be determined.
 */
static bool hw_cpu_is_online(const char *cpu_dir, const cpu_online_map_t *online_map,
                             const char *node_name);

/**
 * Applies a package 0 fallback when no usable CPU topology was discovered.
 *
 * This keeps uniprocessor and embedded ARM systems with no topology sysfs
 * nodes from reporting an empty CPU field.
 *
 * @param cpu_dir Base sysfs CPU directory to scan.
 * @param cpus Array of CPU data structures to update.
 * @param cpu_map Array mapping logical CPU IDs to physical sockets.
 * @param packages Pointer to the total number of CPU packages found.
 * @param freq_available Whether cpufreq attributes may be probed.
 */
static void hw_cpu_apply_single_core_fallback(const char *cpu_dir, cpu_data_t *cpus,
                                              uint16_t *cpu_map, uint8_t *packages,
                                              bool freq_available);

/**
 * Parses /proc/cpuinfo to determine the CPU model name using architecture-specific strategies.
 *
 * @param cpus Array of CPU data structures to update.
 * @param cpu_map Array mapping logical CPU IDs to physical sockets.
 */
static void hw_get_cpu_model(cpu_data_t *cpus, uint16_t *cpu_map);

#if defined(__i386__) || defined(__x86_64__)
/**
 * Parses the x86 processor model name from an open cpuinfo stream.
 *
 * @param fp Pointer to the open /proc/cpuinfo file.
 * @param cpus Array of CPU data structures to update.
 * @param cpu_map Array mapping logical CPU IDs to physical sockets.
 */
static void hw_parse_x86_cpu_model(FILE *fp, cpu_data_t *cpus, uint16_t *cpu_map);

/**
 * Parses out unnecessary marketing jargon from an x86 CPU model string.
 *
 * @param out_buf Buffer containing the CPU model string to sanitize.
 */
static void hw_sanitize_cpu_name(char *out_buf);
#endif

#if defined(__arm__) || defined(__aarch64__)
/**
 * Parses the ARM CPU implementer and part IDs from an open cpuinfo stream.
 *
 * @param fp Pointer to the open /proc/cpuinfo file.
 * @param cpus Array of CPU data structures to update.
 * @param cpu_map Array mapping logical CPU IDs to physical sockets.
 */
static void hw_parse_arm_cpu_model(FILE *fp, cpu_data_t *cpus, uint16_t *cpu_map);

/**
 * Looks up human-readable names for ARM chips based on implementer and part IDs.
 *
 * @param arm_forest The forest containing ARM ID mappings.
 * @param out_buf Buffer containing the raw IDs, replaced with the readable name.
 * @param buf_size Size of the output buffer.
 */
static void hw_arm_lookup_name(const tree_forest_t *arm_forest, char *out_buf,
                               const size_t buf_size);
#endif

/**
 * Enumerates all GPU devices present in sysfs DRM subsystem.
 *
 * @return A linked list of raw GPU nodes, or NULL if none found.
 */
static gpu_node_t *hw_get_all_gpus(void);

/**
 * Maps raw GPU PCI IDs to human-readable names and groups duplicates.
 *
 * @param out_buf Buffer to store the formatted GPU string.
 * @param buf_size Maximum size of the output buffer.
 * @param pci_forest The parsed PCI IDs tree.
 * @param gpu_list Linked list of detected GPUs.
 */
static void hw_gpu_lookup_names(char *out_buf, const size_t buf_size,
                                const tree_forest_t *pci_forest, gpu_node_t *gpu_list);

/**
 * Appends a new GPU to the enumeration list.
 *
 * @param head Pointer to the head of the GPU list.
 * @param impl_id The vendor PCI ID.
 * @param part_id The device PCI ID.
 * @return Pointer to the new head of the list.
 */
static gpu_node_t *add_gpu(gpu_node_t *head, const uint16_t impl_id, const uint16_t part_id);

/**
 * Frees a linked list of GPU nodes.
 *
 * @param head Pointer to the head of the GPU list.
 */
static void free_gpu_list(gpu_node_t *head);

/**
 * Enumerates all power supplies in sysfs to find actual batteries.
 *
 * @return A linked list of battery nodes, or NULL on failure.
 */
static hw_node_t *hw_get_all_batteries(void);

/**
 * Validates a power supply sysfs node to confirm it is a real battery.
 * Filters out AC adapters (non-Battery type) and peripheral devices
 * (scope == Device) such as Bluetooth mice or headphones.
 *
 * @param bat_node_name The name of the power supply directory in sysfs.
 * @return true if the node is a valid system battery, false otherwise.
 */
static bool hw_is_valid_battery(const char *bat_node_name);

/**
 * Reads battery metrics (model, capacity, status) from sysfs for a specific node.
 *
 * @param bat_node_name The directory name of the battery (e.g., "BAT0").
 * @param out_data Pointer to the battery data structure to populate.
 */
static void hw_battery_read_metrics(const char *bat_node_name, bat_data_t *out_data);

/**
 * Appends a new generic hardware element to a list.
 *
 * @param head Pointer to the head of the list.
 * @param name The node name/identifier to store.
 * @return Pointer to the new head of the list.
 */
static hw_node_t *add_element(hw_node_t *head, const char *name);

/**
 * Frees a linked list of generic hardware nodes.
 *
 * @param head Pointer to the head of the list.
 */
static void free_data_list(hw_node_t *head);

/**
 * Formats one CPU package for display.
 *
 * The frequency suffix is only appended when cpufreq data is available, so
 * embedded systems without cpufreq nodes still show a clean model name.
 *
 * @param cpu CPU package data to format.
 * @param out_buf Buffer to store the formatted string.
 * @param buf_size Maximum size of the output buffer.
 * @return true if the package was formatted, false otherwise.
 */
static bool hw_format_cpu_package(cpu_data_t *cpu, char *out_buf, const size_t buf_size)
{
    if (!cpu || !cpu->exists || !out_buf || buf_size == 0) {
        return false;
    }

    uint32_t cores = set_count_bits(&(bitset_t){.bits = cpu->core_bits, .capacity = MAX_CPU_CORES},
                                    CORES_SET_BLOCKS);

    if (cpu->frequency > 0.0) {
        snprintf(out_buf, buf_size, "%.*s (%" PRIu32 ") @ %.03f GHz", (int)sizeof(cpu->model),
                 cpu->model, cores, cpu->frequency);
    } else {
        snprintf(out_buf, buf_size, "%.*s (%" PRIu32 ")", (int)sizeof(cpu->model), cpu->model,
                 cores);
    }

    return true;
}

void hw_get_cpu_info(void)
{
    cpu_data_t cpus[MAX_CPU_PACKAGES] = {0};

    uint16_t cpu_map[MAX_CPU_PACKAGES];
    for (size_t i = 0; i < MAX_CPU_PACKAGES; i++) {
        cpu_map[i] = 0xFFFF;
    }

    uint8_t packages = 0;

    hw_parse_cpu_topology(SYS_CPU_DIR, cpus, cpu_map, &packages);
    hw_get_cpu_model(cpus, cpu_map);

    char cpu_formatted[MAX_CPU_PACKAGES][LINE_BUFFER] = {0};
    const char *cpu_strings[MAX_CPU_PACKAGES] = {0};
    uint8_t formatted_count = 0;

    for (uint8_t i = 0; i < MAX_CPU_PACKAGES; i++) {
        if (!hw_format_cpu_package(&cpus[i], cpu_formatted[formatted_count],
                                   sizeof(cpu_formatted[formatted_count]))) {
            continue;
        }

        cpu_strings[formatted_count] = cpu_formatted[formatted_count];
        formatted_count++;
    }

    char cpu_info[LINE_BUFFER] = {0};
    util_format_duplicate_hardware(cpu_strings, formatted_count, cpu_info, sizeof(cpu_info));

    ui_print_info("CPU", cpu_info);
}

static void hw_parse_cpu_topology(const char *cpu_dir, cpu_data_t *cpus, uint16_t *cpu_map,
                                  uint8_t *packages)
{
    const char *prefix = "cpu";
    size_t prefix_len = strlen(prefix);
    cpu_online_map_t online_map = {0};
    bool freq_available = false;

    hw_cpu_read_online_map(cpu_dir, &online_map);
    freq_available = hw_cpu_has_cpufreq(cpu_dir);

    DIR *dir = opendir(cpu_dir);
    if (!dir) {
        V_PRINTF("[ERROR] Failed to open sysfs CPU directory %s: %s\n", cpu_dir, strerror(errno));
    } else {
        struct dirent *entry = NULL;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, prefix, prefix_len) != 0) {
                continue;
            }

            const char *suffix = entry->d_name + prefix_len;
            if (!util_is_numeric_string(suffix)) {
                continue;
            }

            hw_cpu_process_core_node(cpu_dir, entry->d_name, cpus, cpu_map, packages, &online_map,
                                     freq_available);
        }

        closedir(dir);
    }

    hw_cpu_apply_single_core_fallback(cpu_dir, cpus, cpu_map, packages, freq_available);
}

static void hw_cpu_read_online_map(const char *cpu_dir, cpu_online_map_t *map)
{
    char path[PATH_BUFFER] = {0};
    char line[PATH_BUFFER] = {0};
    const char *cursor = NULL;
    bool ok = true;

    if (cpu_dir == NULL || map == NULL) {
        return;
    }

    map->count = 0;
    map->valid = false;

    if (snprintf(path, sizeof(path), "%sonline", cpu_dir) >= (int)sizeof(path)) {
        return;
    }

    if (!util_read_line(path, line, sizeof(line)) || line[0] == '\0') {
        return;
    }

    cursor = line;
    while (ok && *cursor != '\0') {
        char *end = NULL;
        uintmax_t first = 0;
        uintmax_t last = 0;

        errno = 0;
        first = strtoumax(cursor, &end, 10);
        if (errno != 0 || end == cursor || first > UINT32_MAX) {
            ok = false;
            break;
        }

        cursor = end;
        last = first;

        if (*cursor == '-') {
            errno = 0;
            last = strtoumax(cursor + 1, &end, 10);
            if (errno != 0 || end == cursor + 1 || last < first || last > UINT32_MAX) {
                ok = false;
                break;
            }
            cursor = end;
        }

        if (map->count >= MAX_ONLINE_RANGES) {
            ok = false;
            break;
        }

        map->ranges[map->count].first = (uint32_t)first;
        map->ranges[map->count].last = (uint32_t)last;
        map->count++;

        if (*cursor == ',') {
            cursor++;
        } else if (*cursor != '\0') {
            ok = false;
        }
    }

    if (!ok) {
        map->count = 0;
    }

    map->valid = ok && map->count > 0;
}

static bool hw_cpu_has_cpufreq(const char *cpu_dir)
{
    char path[PATH_BUFFER] = {0};

    if (cpu_dir == NULL) {
        return false;
    }

    if (snprintf(path, sizeof(path), "%scpufreq", cpu_dir) < (int)sizeof(path) &&
        util_is_file_exist(path)) {
        return true;
    }

    if (snprintf(path, sizeof(path), "%scpu0/cpufreq", cpu_dir) < (int)sizeof(path) &&
        util_is_file_exist(path)) {
        return true;
    }

    return false;
}

static bool hw_cpu_is_online(const char *cpu_dir, const cpu_online_map_t *online_map,
                             const char *node_name)
{
    if (cpu_dir == NULL || node_name == NULL) {
        return true;
    }

    if (online_map != NULL && online_map->valid) {
        char *end = NULL;
        uintmax_t cpu_index = strtoumax(node_name + 3, &end, 10);

        if (end != node_name + 3) {
            for (size_t i = 0; i < online_map->count; i++) {
                if (cpu_index >= online_map->ranges[i].first &&
                    cpu_index <= online_map->ranges[i].last) {
                    return true;
                }
            }

            return false;
        }
    }

    char online_path[PATH_BUFFER] = {0};
    snprintf(online_path, sizeof(online_path), "%s%s/online", cpu_dir, node_name);

    if (!util_is_file_exist(online_path)) {
        /* CPU 0 has no hotplug node and is always online. */
        return true;
    }

    uint8_t online = 1;
    if (!util_read_uint8(online_path, &online)) {
        /* Keep the core when the hotplug state cannot be read. */
        return true;
    }

    return online != 0;
}

static void hw_cpu_process_core_node(const char *cpu_dir, const char *node_name, cpu_data_t *cpus,
                                     uint16_t *cpu_map, uint8_t *packages,
                                     const cpu_online_map_t *online_map, bool freq_available)
{
    if (!hw_cpu_is_online(cpu_dir, online_map, node_name)) {
        V_PRINTF("[WARNING] Skipping CPU %s: offline\n", node_name);
        return;
    }

    int16_t physical_package_id = 0;
    uint16_t core_id = 0;
    char full_path[PATH_BUFFER] = {0};

    snprintf(full_path, sizeof(full_path), "%s%s/topology/physical_package_id", cpu_dir, node_name);
    if (!util_read_int16(full_path, &physical_package_id)) {
        V_PRINTF("[WARNING] CPU %s: missing physical package ID, "
                 "falling back to package 0\n",
                 node_name);
        physical_package_id = 0;
    }

    if (physical_package_id < 0 || physical_package_id >= MAX_CPU_PACKAGES) {
        V_PRINTF("[WARNING] CPU %s: invalid physical package ID %" PRId16 ", "
                 "falling back to package 0\n",
                 node_name, physical_package_id);
        physical_package_id = 0;
    }

    snprintf(full_path, sizeof(full_path), "%s%s/topology/core_id", cpu_dir, node_name);

    uint16_t current_id = (uint16_t)strtoul(node_name + 3, NULL, 10);
    if (!util_read_uint16(full_path, &core_id)) {
        V_PRINTF("[WARNING] CPU %s: missing core ID, using logical ID\n", node_name);
        core_id = current_id;
    }

    if (!cpus[physical_package_id].exists) {
        cpus[physical_package_id].exists = true;
        (*packages)++;
    }

    if (cpu_map[physical_package_id] == 0xFFFF || current_id < cpu_map[physical_package_id]) {
        cpu_map[physical_package_id] = current_id;
    }

    set_add(&(bitset_t){.bits = cpus[physical_package_id].core_bits, .capacity = MAX_CPU_CORES},
            core_id);

    if (freq_available) {
        hw_get_cpu_freq(cpu_dir, cpus, physical_package_id, node_name);
    }
}

static void hw_cpu_apply_single_core_fallback(const char *cpu_dir, cpu_data_t *cpus,
                                              uint16_t *cpu_map, uint8_t *packages,
                                              bool freq_available)
{
    if (*packages > 0 && cpus[0].exists) {
        return;
    }

    char cpu0_path[PATH_BUFFER] = {0};
    snprintf(cpu0_path, sizeof(cpu0_path), "%scpu0", cpu_dir);

    if (!util_is_file_exist(cpu0_path) && !util_is_file_exist(CPUINFO_PATH)) {
        return;
    }

    if (!cpus[0].exists) {
        cpus[0].exists = true;
        (*packages)++;
    }

    set_add(&(bitset_t){.bits = cpus[0].core_bits, .capacity = MAX_CPU_CORES}, 0);

    if (cpu_map[0] == 0xFFFF) {
        cpu_map[0] = 0;
    }
    if (*packages == 0) {
        *packages = 1;
    }

    if (freq_available) {
        hw_get_cpu_freq(cpu_dir, cpus, 0, "cpu0");
    }
}

/**
 * Reads the first available frequency from a set of cpufreq sysfs attributes.
 *
 * Both the per-CPU cpufreq directory and the policy-based cpufreq directory
 * are probed for each attribute before moving on to the next one.
 *
 * @param cpu_dir Base sysfs CPU directory to scan.
 * @param cpu_name Directory name of the CPU node (e.g., "cpu0").
 * @param freq_files NULL-terminated list of attribute names to query.
 * @return Frequency in kHz, or 0 if none could be read.
 */
static uint32_t hw_read_cpu_freq_files(const char *cpu_dir, const char *cpu_name,
                                       const char *const freq_files[])
{
    char freq_path[PATH_BUFFER] = {0};
    uint32_t cpu_index = (uint32_t)strtoul(cpu_name + 3, NULL, 10);

    for (size_t i = 0; freq_files[i] != NULL; i++) {
        uint32_t freq_khz = 0;

        snprintf(freq_path, sizeof(freq_path), "%s%s/cpufreq/%s", cpu_dir, cpu_name, freq_files[i]);
        if (util_read_uint32(freq_path, &freq_khz) && freq_khz > 0) {
            return freq_khz;
        }

        snprintf(freq_path, sizeof(freq_path), "%scpufreq/policy%" PRIu32 "/%s", cpu_dir, cpu_index,
                 freq_files[i]);
        if (util_read_uint32(freq_path, &freq_khz) && freq_khz > 0) {
            return freq_khz;
        }
    }

    return 0;
}

static void hw_get_cpu_freq(const char *cpu_dir, cpu_data_t *cpus, const int16_t package,
                            const char *cpu_name)
{
    uint32_t freq_khz = hw_read_cpu_freq_files(cpu_dir, cpu_name, CPU_FREQ_FILES);
    double frequency = (double)freq_khz / KHZ_PER_GHZ;

    if (frequency > cpus[package].frequency) {
        cpus[package].frequency = frequency;
    }
}

static bool hw_cpu_models_complete(const cpu_data_t *cpus)
{
    bool found = false;

    for (uint8_t i = 0; i < MAX_CPU_PACKAGES; i++) {
        if (!cpus[i].exists) {
            continue;
        }

        found = true;
        if (cpus[i].model[0] == '\0') {
            return false;
        }
    }

    return found;
}

static void hw_get_cpu_model(cpu_data_t *cpus, uint16_t *cpu_map)
{
    FILE *fp = fopen(CPUINFO_PATH, "r");
    if (!fp) {
        V_PRINTF("[ERROR] Failed to open CPU info file: %s\n", strerror(errno));
        return;
    }

    /*
     * /proc/cpuinfo is synthetic and its default stdio buffer is often only
     * one kernel page. A larger buffer avoids dozens of tiny read() calls.
     */
    static char io_buffer[8192];
    setvbuf(fp, io_buffer, _IOFBF, sizeof(io_buffer));

#if defined(__i386__) || defined(__x86_64__)
    hw_parse_x86_cpu_model(fp, cpus, cpu_map);
#elif defined(__arm__) || defined(__aarch64__)
    hw_parse_arm_cpu_model(fp, cpus, cpu_map);
#endif

    fclose(fp);
}

#if defined(__i386__) || defined(__x86_64__)
static void hw_parse_x86_cpu_model(FILE *fp, cpu_data_t *cpus, uint16_t *cpu_map)
{
    char line[LINE_BUFFER] = {0};
    int8_t socket = -1;

    while (fgets(line, sizeof(line), fp)) {
        char *delim = strchr(line, ':');
        if (!delim) {
            continue;
        }

        *delim = 0;
        char *key = line;
        char *value = delim + 1;

        value[strcspn(value, "\n")] = 0;

        if (value[0] == ' ') {
            value++;
        }

        if (strncmp(key, "processor", 9) == 0) {
            socket = -1;

            for (uint8_t i = 0; i < MAX_CPU_PACKAGES; i++) {
                if (cpus[i].exists && cpu_map[i] == strtoul(value, NULL, 10)) {
                    socket = (int8_t)i;
                    break;
                }
            }
        }

        if (strncmp(key, "model name", 10) == 0) {
            if (socket == -1 || cpus[socket].model[0] != '\0') {
                continue;
            }
            snprintf(cpus[socket].model, sizeof(cpus[socket].model), "%s", value);
            hw_sanitize_cpu_name(cpus[socket].model);

            if (hw_cpu_models_complete(cpus)) {
                break;
            }
        }
    }
}

/**
 * Removes a standalone word and its adjacent separator from a CPU model string.
 *
 * @param out_buf Buffer containing the CPU model string to update.
 * @param word The standalone word to remove.
 */
static void hw_remove_cpu_word(char *out_buf, const char *word)
{
    size_t word_len = strlen(word);
    char *pos = out_buf;

    while ((pos = strstr(pos, word)) != NULL) {
        bool left_boundary = (pos == out_buf) || (pos[-1] == ' ');
        bool right_boundary = (pos[word_len] == '\0') || (pos[word_len] == ' ');

        if (!left_boundary || !right_boundary) {
            pos += word_len;
            continue;
        }

        char *start = pos;
        char *end = pos + word_len;

        if (*end == '\0') {
            while (start > out_buf && start[-1] == ' ') {
                start--;
            }
        } else {
            while (*end == ' ') {
                end++;
            }
        }

        memmove(start, end, strlen(end) + 1);
        pos = start;
    }
}

/**
 * Truncates a CPU model string before a trailing marketing suffix.
 *
 * @param out_buf Buffer containing the CPU model string to update.
 * @param suffix The suffix that marks the start of removable text.
 */
static void hw_truncate_cpu_suffix(char *out_buf, const char *suffix)
{
    char *garbage = strstr(out_buf, suffix);
    if (!garbage || garbage == out_buf) {
        return;
    }

    while (garbage > out_buf && garbage[-1] != ' ') {
        garbage--;
    }

    if (garbage > out_buf) {
        garbage--;
    }

    *garbage = '\0';
}

static void hw_sanitize_cpu_name(char *out_buf)
{
    if (!out_buf || !*out_buf) {
        return;
    }

    /* Fucking piece of shit that needs its own fucking line */
    char *garbage_gen = strstr(out_buf, "Gen");
    if (garbage_gen) {
        char *space_after = strchr(garbage_gen, ' ');
        if (space_after) {
            space_after++;
            memmove(out_buf, space_after, strlen(space_after) + 1);
        }
    }

    const char *prefix_garbage[] = {"(R)", "(TM)", "(tm)", NULL};

    size_t i = 0;
    while (prefix_garbage[i] != NULL) {
        char *garbage = strstr(out_buf, prefix_garbage[i]);
        if (garbage) {
            char *src = garbage + strlen(prefix_garbage[i]);
            size_t len = strlen(src) + 1;
            memmove(garbage, src, len);
        } else {
            i++;
        }
    }

    hw_remove_cpu_word(out_buf, "CPU");
    hw_remove_cpu_word(out_buf, "APU");

    const char *suffix_garbage[] = {"with", "-Core", "Processor", NULL};

    for (size_t j = 0; suffix_garbage[j] != NULL; j++) {
        hw_truncate_cpu_suffix(out_buf, suffix_garbage[j]);
    }

    char *frequency = strstr(out_buf, " @");
    if (frequency) {
        *frequency = '\0';
    }

    size_t len = strlen(out_buf);
    while (len > 0 && out_buf[len - 1] == ' ') {
        out_buf[--len] = '\0';
    }
}
#endif

#if defined(__arm__) || defined(__aarch64__)
static void hw_parse_arm_cpu_model(FILE *fp, cpu_data_t *cpus, uint16_t *cpu_map)
{
    tree_forest_t *arm_forest =
        util_parse_ids_buffer((const char *)arm_ids, arm_ids_len, ARM_IMPLEMENTERS);
    if (arm_forest == NULL) {
        V_PRINTF("[WARNING] Failed to build an ARM forest. Using raw values\n");
    }

    char arm_model[TINY_BUFFER] = {0};
    char line[LINE_BUFFER] = {0};
    int8_t socket = -1;

    while (fgets(line, sizeof(line), fp)) {
        char *delim = strchr(line, ':');
        if (!delim) {
            continue;
        }

        *delim = 0;
        char *key = line;
        char *value = delim + 1;

        value[strcspn(value, "\n")] = 0;

        if (value[0] == ' ') {
            value++;
        }

        if (strncmp(key, "processor", 9) == 0) {
            arm_model[0] = '\0';
            socket = -1;

            for (uint8_t i = 0; i < MAX_CPU_PACKAGES; i++) {
                if (cpus[i].exists && cpu_map[i] == strtoul(value, NULL, 10)) {
                    socket = (int8_t)i;
                    break;
                }
            }
        }

        if (strncmp(key, "CPU implementer", 15) == 0) {
            if (socket == -1 || cpus[socket].model[0] != '\0') {
                continue;
            }

            snprintf(arm_model, sizeof(arm_model), "%s ", value);
        }

        if (strncmp(key, "CPU part", 8) == 0) {
            if (socket == -1 || cpus[socket].model[0] != '\0') {
                continue;
            }

            size_t used = strlen(arm_model);
            if (used < sizeof(arm_model)) {
                snprintf(arm_model + used, sizeof(arm_model) - used, "%s", value);
            }

            snprintf(cpus[socket].model, sizeof(cpus[socket].model), "%s", arm_model);

            hw_arm_lookup_name(arm_forest, cpus[socket].model, sizeof(cpus[socket].model));

            if (hw_cpu_models_complete(cpus)) {
                break;
            }
        }
    }

    tree_destroy_forest(arm_forest);
}

static void hw_arm_lookup_name(const tree_forest_t *arm_forest, char *out_buf,
                               const size_t buf_size)
{
    if (!out_buf || buf_size == 0) {
        V_PRINTF("[ERROR] hw_arm_lookup_name: incorrect arguments "
                 "(buf = %p, size = %zu)\n",
                 (void *)out_buf, buf_size);
        return;
    }

    char *endptr = NULL;
    uint8_t vendor_id = (uint8_t)strtoul(out_buf, &endptr, 16);
    uint16_t device_id = (uint16_t)strtoul(endptr, NULL, 16);

    if (arm_forest) {
        tree_node_t *vendor = tree_find_root(arm_forest, vendor_id);
        const tree_node_t *device = tree_find(vendor, device_id);

        if (vendor && device) {
            snprintf(out_buf, buf_size, "%s %s", vendor->name, device->name);
            return;
        } else if (vendor) {
            snprintf(out_buf, buf_size, "%s [0x%04" PRIX16 "]", vendor->name, device_id);
            return;
        }
    }

    snprintf(out_buf, buf_size, "Generic ARM CPU [0x%04" PRIX16 " 0x%04" PRIX16 "]", vendor_id,
             device_id);
}
#endif

void hw_get_gpu_info(void)
{
    gpu_node_t *gpu_list = NULL;
    tree_forest_t *pci_forest = NULL;
    util_ids_query_t queries[MAX_GPUS] = {0};
    size_t query_count = 0;
    char info_buf[GPU_BUFFER] = {0};

    const char *paths[] = PCI_IDS_PATHS;
    const char *pci_path = NULL;

    gpu_list = hw_get_all_gpus();
    if (gpu_list == NULL) {
#if defined(__i386__) || defined(__x86_64__) || defined(__ia64__) || defined(__powerpc64__)
        V_PRINTF("[WARNING] No GPUs found in system (might be VM?)\n");
#endif
        goto cleanup;
    }

    for (gpu_node_t *curr = gpu_list; curr != NULL && query_count < MAX_GPUS; curr = curr->next) {
        queries[query_count].vendor_id = curr->impl_id;
        queries[query_count].device_id = curr->part_id;
        query_count++;
    }

    for (size_t i = 0; paths[i] != NULL; i++) {
        if (util_is_file_exist(paths[i])) {
            pci_path = paths[i];
            break;
        }
    }

    if (!pci_path) {
        V_PRINTF("[ERROR] Failed to find pci.ids file\n");
    }

    pci_forest = util_parse_ids_file_filtered(pci_path, MAX_GPUS, queries, query_count);

    if (pci_forest == NULL) {
        V_PRINTF("[WARNING] Failed to build a PCI forest. Using raw values\n");
    }

    hw_gpu_lookup_names(info_buf, sizeof(info_buf), pci_forest, gpu_list);

    if (info_buf[0] != '\0') {
        ui_print_info("GPU", info_buf);
    }

cleanup:
    if (pci_forest) {
        tree_destroy_forest(pci_forest);
    }
    if (gpu_list) {
        free_gpu_list(gpu_list);
    }
}

static gpu_node_t *hw_get_all_gpus(void)
{
    const char *prefix = "card";
    size_t prefix_len = strlen("card");
    DIR *dir = NULL;
    gpu_node_t *head = NULL;
    struct dirent *entry = NULL;

    dir = opendir(SYS_GPU_DIR);
    if (!dir) {
        V_PRINTF("[ERROR] Failed to open GPU directory %s: %s\n", SYS_GPU_DIR, strerror(errno));
        goto cleanup;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, prefix, prefix_len) != 0) {
            continue;
        }

        const char *suffix = entry->d_name + prefix_len;
        if (!util_is_numeric_string(suffix)) {
            continue;
        }

        uint16_t vendor_id = 0, device_id = 0;
        char full_path[PATH_BUFFER] = {0};

        snprintf(full_path, sizeof(full_path), "%s%s/device/vendor", SYS_GPU_DIR, entry->d_name);
        util_read_hex16(full_path, &vendor_id);

        snprintf(full_path, sizeof(full_path), "%s%s/device/device", SYS_GPU_DIR, entry->d_name);
        util_read_hex16(full_path, &device_id);

        head = add_gpu(head, vendor_id, device_id);
    }

cleanup:
    if (dir) {
        closedir(dir);
    }
    return head;
}

static void hw_gpu_lookup_names(char *out_buf, const size_t buf_size,
                                const tree_forest_t *pci_forest, gpu_node_t *gpu_list)
{
    if (!out_buf || buf_size == 0) {
        V_PRINTF("[ERROR] hw_gpu_lookup_names: incorrect arguments "
                 "(buf = %p, size = %zu)\n",
                 (void *)out_buf, buf_size);
        return;
    }

    gpu_data_t gpus[MAX_GPUS] = {0};
    uint8_t gpu_count = 0;

    for (gpu_node_t *curr = gpu_list; curr != NULL && gpu_count < MAX_GPUS; curr = curr->next) {
        const char *vendor_name = "Generic GPU";
        const char *device_name = "";
        const char *separator = "";

        if (pci_forest) {
            tree_node_t *vendor = tree_find_root(pci_forest, curr->impl_id);
            const tree_node_t *device = tree_find(vendor, curr->part_id);

            if (vendor) {
                vendor_name = vendor->name;
            }

            if (device) {
                device_name = device->name;
                separator = " ";
            }
        }

        snprintf(gpus[gpu_count].model, sizeof(gpus[gpu_count].model), "%s%s%s [0x%04" PRIX16 "]",
                 vendor_name, separator, device_name, curr->part_id);
        gpu_count++;
    }

    const char *gpu_strings[MAX_GPUS] = {0};
    for (uint8_t i = 0; i < gpu_count; i++) {
        gpu_strings[i] = gpus[i].model;
    }

    util_format_duplicate_hardware(gpu_strings, gpu_count, out_buf, buf_size);
}

static gpu_node_t *add_gpu(gpu_node_t *head, const uint16_t impl_id, const uint16_t part_id)
{
    gpu_node_t *new_node = malloc(sizeof(gpu_node_t));
    if (!new_node) {
        return head;
    }

    memset(new_node, 0, sizeof(gpu_node_t));

    new_node->impl_id = impl_id;
    new_node->part_id = part_id;
    new_node->next = head;

    return new_node;
}

static void free_gpu_list(gpu_node_t *head)
{
    while (head) {
        gpu_node_t *temp = head;
        head = head->next;
        free(temp);
    }
}

void hw_get_bat_info(void)
{
    hw_node_t *list = hw_get_all_batteries();
    if (list == NULL) {
        return;
    }

    for (hw_node_t *curr = list; curr != NULL; curr = curr->next) {
        bat_data_t bat_info = {0};
        hw_battery_read_metrics(curr->name, &bat_info);

        char label_buf[MEDIUM_BUFFER] = {0};
        snprintf(label_buf, sizeof(label_buf), "Battery (%s)",
                 (bat_info.model[0] == '\0') ? "Unknown" : bat_info.model);

        const char *health = NULL;
        if (bat_info.health & HEALTH_GOOD) {
            health = "Good";
        } else if (bat_info.health & HEALTH_FAIR) {
            health = "Fair";
        } else if (bat_info.health & HEALTH_POOR) {
            health = "Poor";
        } else {
            health = "Unknown";
        }

        char info_buf[MEDIUM_BUFFER] = {0};
        snprintf(info_buf, sizeof(info_buf), "%s%% (%s, Health: %s)", bat_info.capacity,
                 (bat_info.status[0] == '\0') ? "Unknown" : bat_info.status, health);

        ui_print_info(label_buf, info_buf);
    }

    free_data_list(list);
}

static hw_node_t *hw_get_all_batteries(void)
{
    DIR *dir = NULL;
    hw_node_t *head = NULL;
    struct dirent *entry = NULL;

    dir = opendir(SYS_BATTERY_DIR);
    if (!dir) {
        goto cleanup;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        if (!hw_is_valid_battery(entry->d_name)) {
            continue;
        }

        head = add_element(head, entry->d_name);
    }

cleanup:
    if (dir) {
        closedir(dir);
    }
    return head;
}

static bool hw_is_valid_battery(const char *bat_node_name)
{
    char sysfs_path[PATH_BUFFER] = {0};
    snprintf(sysfs_path, sizeof(sysfs_path), SYS_BATTERY_DIR "%s/type", bat_node_name);

    char type[TINY_BUFFER] = {0};
    if (util_read_line(sysfs_path, type, sizeof(type)) && strncasecmp(type, "Battery", 7) != 0) {
        return false;
    }

    char scope[TINY_BUFFER] = {0};
    snprintf(sysfs_path, sizeof(sysfs_path), SYS_BATTERY_DIR "%s/scope", bat_node_name);
    if (util_read_line(sysfs_path, scope, sizeof(scope)) && strncasecmp(scope, "Device", 6) == 0) {
        return false;
    }

    return true;
}

static void hw_battery_read_metrics(const char *bat_node_name, bat_data_t *out_data)
{
    char full_path[PATH_BUFFER] = {0};

    snprintf(full_path, sizeof(full_path), SYS_BATTERY_DIR "%s/model_name", bat_node_name);
    util_read_line(full_path, out_data->model, sizeof(out_data->model));

    snprintf(full_path, sizeof(full_path), SYS_BATTERY_DIR "%s/capacity", bat_node_name);
    util_read_line(full_path, out_data->capacity, sizeof(out_data->capacity));

    snprintf(full_path, sizeof(full_path), SYS_BATTERY_DIR "%s/status", bat_node_name);
    util_read_line(full_path, out_data->status, sizeof(out_data->status));

    int32_t energy_full_design = 0;
    snprintf(full_path, sizeof(full_path), SYS_BATTERY_DIR "%s/energy_full_design", bat_node_name);
    if (!util_read_int32(full_path, &energy_full_design)) {
        snprintf(full_path, sizeof(full_path), SYS_BATTERY_DIR "%s/charge_full_design",
                 bat_node_name);
        util_read_int32(full_path, &energy_full_design);
    }

    int32_t energy_full = 0;
    snprintf(full_path, sizeof(full_path), SYS_BATTERY_DIR "%s/energy_full", bat_node_name);
    if (!util_read_int32(full_path, &energy_full)) {
        snprintf(full_path, sizeof(full_path), SYS_BATTERY_DIR "%s/charge_full", bat_node_name);
        util_read_int32(full_path, &energy_full);
    }

    uint8_t health = 0;
    bool health_ok = false;

    if (energy_full_design > 0 && energy_full > 0) {
        double ratio = (double)energy_full / (double)energy_full_design * 100.0;
        if (ratio > 100.0) {
            ratio = 100.0;
        }

        health = (uint8_t)ratio;
        health_ok = true;
    }

    if (!health_ok) {
        out_data->health |= HEALTH_UNKNOWN;
    } else if (health >= 80) {
        out_data->health |= HEALTH_GOOD;
    } else if (health >= 60) {
        out_data->health |= HEALTH_FAIR;
    } else {
        out_data->health |= HEALTH_POOR;
    }
}

static hw_node_t *add_element(hw_node_t *head, const char *name)
{
    hw_node_t *new_node = malloc(sizeof(hw_node_t));
    if (!new_node) {
        return head;
    }

    memset(new_node, 0, sizeof(hw_node_t));

    size_t name_len = strlen(name);
    size_t to_copy = (name_len >= sizeof(new_node->name)) ? sizeof(new_node->name) - 1 : name_len;

    memcpy(new_node->name, name, to_copy);
    new_node->name[to_copy] = '\0';

    new_node->next = head;
    return new_node;
}

static void free_data_list(hw_node_t *head)
{
    while (head) {
        hw_node_t *temp = head;
        head = head->next;
        free(temp);
    }
}
