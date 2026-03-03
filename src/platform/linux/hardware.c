/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#include "pal/hardware_os.h"

/* Array of paths to search for the pci.ids database file. */
#define PCI_IDS_PATHS {            \
    "/var/lib/pciutils/pci.ids",   \
    "/usr/share/hwdata/pci.ids",   \
    "/usr/share/misc/pci.ids",     \
    "/usr/share/pci.ids",          \
    "/opt/homebrew/share/pci.ids", \
    "pci.ids",                     \
    NULL                           \
}

/* Base directories in sysfs and procfs for hardware enumeration. */
#define SYS_CPU_DIR      "/sys/devices/system/cpu/"
#define CPUINFO_PATH     "/proc/cpuinfo"
#define SYS_GPU_DIR      "/sys/class/drm/"
#define SYS_BATTERY_DIR  "/sys/class/power_supply/"

/* Maximum supported limits for CPU discovery. */
#define MAX_CPU_PACKAGES 16
#define MAX_CPU_CORES    256

/* Number of blocks needed for the CPU cores bitset. */
#define CORES_SET_BLOCKS (MAX_CPU_CORES / BITS_PER_BLOCK)

/* Internal structure to store CPU parsing progress per socket. */
typedef struct {
    char model[MEDIUM_BUFFER];
    double frequency;
    uint32_t core_bits[CORES_SET_BLOCKS];
    bool exists;
} cpu_data_t;

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

/**
 * Recursively parses the sysfs directory to construct the CPU topology.
 *
 * @param cpus Array of CPU data structures to populate.
 * @param cpu_map Array mapping logical CPU IDs to physical sockets.
 * @param packages Pointer to store the total number of CPU packages found.
 */
static void hw_parse_cpu_topology(cpu_data_t *cpus, uint16_t *cpu_map, uint8_t *packages);

/**
 * Parses hardware properties of a single logical CPU core node.
 * 
 * @param node_name The directory name of the CPU node (e.g., "cpu0").
 * @param cpus Array of CPU data structures to be updated.
 * @param cpu_map Array mapping logical CPU IDs to physical sockets.
 * @param packages Pointer to the total number of CPU packages found.
 */
static void hw_cpu_process_core_node(const char *node_name, cpu_data_t *cpus,
                                     uint16_t *cpu_map, uint8_t *packages);

/**
 * Reads the current scaling frequency for a given CPU package.
 *
 * @param cpus Array of CPU data structures.
 * @param package The physical package ID.
 * @param cpu_suffix The string identifying the CPU node in sysfs.
 */
static void hw_get_cpu_freq(cpu_data_t *cpus, const int16_t package, const char *cpu_suffix);

/**
 * Parses /proc/cpuinfo to determine the CPU model name using architecture-specific strategies.
 *
 * @param cpus Array of CPU data structures to update.
 * @param cpu_map Array mapping logical CPU IDs to physical sockets.
 * @param packages Total number of CPU packages in the system.
 */
static void hw_get_cpu_model(cpu_data_t *cpus, uint16_t *cpu_map, const uint8_t packages);

#if defined(__i386__) || defined(__x86_64__)
/**
 * Parses the x86 processor model name from an open cpuinfo stream.
 * 
 * @param fp Pointer to the open /proc/cpuinfo file.
 * @param cpus Array of CPU data structures to update.
 * @param cpu_map Array mapping logical CPU IDs to physical sockets.
 * @param packages Total number of CPU packages.
 */
static void hw_parse_x86_cpu_model(FILE *fp, cpu_data_t *cpus,
                                   uint16_t *cpu_map, const uint8_t packages);

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
 * @param packages Total number of CPU packages.
 */
static void hw_parse_arm_cpu_model(FILE *fp, cpu_data_t *cpus,
                                   uint16_t *cpu_map, const uint8_t packages);

/**
 * Creates a binary tree forest of ARM vendor and part IDs from an internal database.
 *
 * @return A pointer to the populated forest, or NULL on failure.
 */
static forest *hw_arm_forest_create(void);

/**
 * Looks up human-readable names for ARM chips based on implementer and part IDs.
 *
 * @param arm_forest The forest containing ARM ID mappings.
 * @param out_buf Buffer containing the raw IDs, replaced with the readable name.
 * @param buf_size Size of the output buffer.
 */
static void hw_arm_lookup_name(forest *arm_forest, char *out_buf, const size_t buf_size);
#endif

/**
 * Enumerates all GPU devices present in sysfs DRM subsystem.
 *
 * @return A linked list of raw GPU nodes, or NULL if none found.
 */
static gpu_node_t *hw_get_all_gpus(void);

/**
 * Parses the pci.ids database file into a binary tree forest.
 *
 * @return A tree containing PCI vendors and sub-devices, or NULL on failure.
 */
static forest *hw_pci_forest_create(void);

/**
 * Maps raw GPU PCI IDs to human-readable names and groups duplicates.
 *
 * @param out_buf Buffer to store the formatted GPU string.
 * @param buf_size Maximum size of the output buffer.
 * @param pci_forest The parsed PCI IDs tree.
 * @param gpu_list Linked list of detected GPUs.
 */
static void hw_gpu_lookup_names(char *out_buf, const size_t buf_size,
                                forest *pci_forest, gpu_node_t *gpu_list);

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

void hw_get_cpu_info(void) {
    cpu_data_t cpus[MAX_CPU_PACKAGES];
    memset(cpus, 0, sizeof(cpus));

    uint16_t cpu_map[MAX_CPU_PACKAGES];
    memset(cpu_map, 0xFFFF, sizeof(cpu_map));

    uint8_t packages = 0;

    hw_parse_cpu_topology(cpus, cpu_map, &packages);
    hw_get_cpu_model(cpus, cpu_map, packages);

    char cpu_formatted[MAX_CPU_PACKAGES][LINE_BUFFER] = {0};
    const char *cpu_strings[MAX_CPU_PACKAGES] = {0};
    for (uint8_t i = 0; i < packages; i++) {
        uint32_t cores = count_set_bits(&(bitset_t){.bits = cpus[i].core_bits,
                                                    .capacity = MAX_CPU_CORES}, 8);

        snprintf(cpu_formatted[i], sizeof(cpu_formatted[i]),
                 "%s (%" PRIu32 ") @ %.03f GHz", cpus[i].model, cores, cpus[i].frequency);

        cpu_strings[i] = cpu_formatted[i];
    }

    char cpu_info[LINE_BUFFER] = {0};
    util_format_duplicate_hardware(cpu_strings, packages, cpu_info, sizeof(cpu_info));

    ui_print_info("CPU", cpu_info);
}

static void hw_parse_cpu_topology(cpu_data_t *cpus, uint16_t *cpu_map, uint8_t *packages) {
    const char *prefix = "cpu";
    size_t prefix_len = strlen(prefix);

    DIR *dir = opendir(SYS_CPU_DIR);
    if (!dir) {
        V_PRINTF("[ERROR] Failed to open sysfs CPU directory %s: %s\n",
                 SYS_CPU_DIR, strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, prefix, prefix_len) != 0) continue;

        const char *suffix = entry->d_name + prefix_len;
        if (!util_is_numeric_string(suffix)) continue;

        hw_cpu_process_core_node(entry->d_name, cpus, cpu_map, packages);
    }

    closedir(dir);
}

static void hw_cpu_process_core_node(const char *node_name, cpu_data_t *cpus,
                                     uint16_t *cpu_map, uint8_t *packages) {
    int16_t physical_package_id = 0;
    uint16_t core_id = 0;
    char full_path[PATH_BUFFER] = {0};

    snprintf(full_path, sizeof(full_path),
             "%s%s/topology/physical_package_id", SYS_CPU_DIR, node_name);
    util_read_int16(full_path, &physical_package_id);
    if (physical_package_id < 0 || physical_package_id >= MAX_CPU_PACKAGES) {
        physical_package_id = 0;
    }

    if (!cpus[physical_package_id].exists) {
        cpus[physical_package_id].exists = true;
        (*packages)++;

        hw_get_cpu_freq(cpus, physical_package_id, node_name);
    }

    uint16_t current_id = strtoul(node_name + 3, NULL, 10);
    if (cpu_map[physical_package_id] == 0xFFFF || current_id < cpu_map[physical_package_id]) {
        cpu_map[physical_package_id] = current_id;
    }

    snprintf(full_path, sizeof(full_path), "%s%s/topology/core_id", SYS_CPU_DIR, node_name);
    util_read_uint16(full_path, &core_id);
    set_add(&(bitset_t){.bits = cpus[physical_package_id].core_bits,
                        .capacity = MAX_CPU_CORES}, core_id);
}

static void hw_get_cpu_freq(cpu_data_t *cpus, const int16_t package, const char *cpu_suffix) {
    char freq_path[PATH_BUFFER] = {0};
    snprintf(freq_path, sizeof(freq_path),
             "%s%s/cpufreq/scaling_cur_freq", SYS_CPU_DIR, cpu_suffix);

    uint32_t freq_khz = 0;
    util_read_uint32(freq_path, &freq_khz);

    if (freq_khz == 0) {
        snprintf(freq_path, sizeof(freq_path),
                 "%s%s/cpufreq/cpuinfo_cur_freq", SYS_CPU_DIR, cpu_suffix);
        util_read_uint32(freq_path, &freq_khz);
    }

    cpus[package].frequency = (double)freq_khz / KHZ_PER_GHZ;
}

static void hw_get_cpu_model(cpu_data_t *cpus, uint16_t *cpu_map, const uint8_t packages) {
    FILE *fp = fopen(CPUINFO_PATH, "r");
    if (!fp) {
        V_PRINTF("[ERROR] Failed to open CPU info file: %s\n", strerror(errno));
        return;
    }

#if defined(__i386__) || defined(__x86_64__)
    hw_parse_x86_cpu_model(fp, cpus, cpu_map, packages);
#elif defined(__arm__) || defined(__aarch64__)
    hw_parse_arm_cpu_model(fp, cpus, cpu_map, packages);
#endif 
}

#if defined(__i386__) || defined(__x86_64__)
static void hw_parse_x86_cpu_model(FILE *fp, cpu_data_t *cpus,
                                   uint16_t *cpu_map, const uint8_t packages) {
    char line[LINE_BUFFER] = {0};
    int8_t socket = -1;

    while (fgets(line, sizeof(line), fp)) {
        char *delim = strchr(line, ':');
        if (!delim) continue;

        *delim = 0;
        char *key = line;
        char *value = delim + 1;

        value[strcspn(value, "\n")] = 0;

        if (value[0] == ' ') value++;

        if (strncmp(key, "processor", 9) == 0) {
            socket = -1;

            for (uint8_t i = 0; i < packages; i++) {
                if (cpu_map[i] == strtoul(value, NULL, 10)) {
                    socket = i;
                    break;
                }
            }
        }

        if (strncmp(key, "model name", 10) == 0) {
            if (socket == -1 || cpus[socket].model[0] != '\0') continue;
            memcpy(cpus[socket].model, value, strlen(value));
            hw_sanitize_cpu_name(cpus[socket].model);
        }
    }
}

static void hw_sanitize_cpu_name(char *out_buf) {
    if (!out_buf || !*out_buf) return;

    /* Fucking piece of shit that needs its own fucking line */
    char *garbage = strstr(out_buf, "Gen");
    if (garbage) {
        while (*garbage != ' ') garbage++;
        garbage++;
        uint8_t offset = garbage - out_buf;
        memmove(out_buf, out_buf + offset, strlen(garbage) + 1);
    }

    const char *prefix_garbage[] = {"(R)", "(TM)", "(tm)", NULL};

    int i = 0;
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

    const char *suffix_garbage[] = {"CPU", "APU", "with", "-Core", "Processor", NULL};

    for (uint8_t i = 0; suffix_garbage[i] != NULL; i++) {
        char *garbage = strstr(out_buf, suffix_garbage[i]);
        if (garbage) {
            while (garbage[0] != ' ') garbage--;
            *garbage = '\0';
            break;
        }
    }
}
#endif

#if defined(__arm__) || defined(__aarch64__)
static void hw_parse_arm_cpu_model(FILE *fp, cpu_data_t *cpus,
                                   uint16_t *cpu_map, const uint8_t packages) {
    forest *arm_forest = hw_arm_forest_create();
    if (arm_forest == NULL) {
        V_PRINTF("[WARNING] Failed to build an ARM forest. Using raw values\n");
    }

    char arm_model[TINY_BUFFER] = {0};
    char line[LINE_BUFFER] = {0};
    int8_t socket = -1;

    while (fgets(line, sizeof(line), fp)) {
        char *delim = strchr(line, ':');
        if (!delim) continue;

        *delim = 0;
        char *key = line;
        char *value = delim + 1;

        value[strcspn(value, "\n")] = 0;

        if (value[0] == ' ') value++;

        if (strncmp(key, "processor", 9) == 0) {
            arm_model[0] = '\0';
            socket = -1;

            for (uint8_t i = 0; i < packages; i++) {
                if (cpu_map[i] == strtoul(value, NULL, 10)) {
                    socket = i;
                    break;
                }
            }
        }

        if (strncmp(key, "CPU implementer", 15) == 0) {
            if (socket == -1 || cpus[socket].model[0] != '\0') continue;

            size_t impl_len = strlen(value);
            memcpy(arm_model, value, impl_len);
            arm_model[impl_len] = ' ';
            arm_model[impl_len + 1] = '\0';
        }

        if (strncmp(key, "CPU part", 8) == 0) {
            if (socket == -1 || cpus[socket].model[0] != '\0') continue;

            char *dst = strstr(arm_model, " ");
            if (dst) memcpy(dst + 1, value, strlen(value) + 1);

            size_t model_len = strlen(arm_model);
            memcpy(cpus[socket].model, arm_model, model_len);
            cpus[socket].model[model_len] = '\0';

            hw_arm_lookup_name(arm_forest, cpus[socket].model, sizeof(cpus[socket].model));
        }
    }

    destroy_forest(arm_forest);
}

static forest *hw_arm_forest_create(void) {
    FILE *fp = fmemopen(arm_ids, arm_ids_len, "r");
    if (!fp) {
        V_PRINTF("[ERROR] Failed to open arm.ids file: %s\n", strerror(errno));
        return NULL;
    }

    forest *arm_forest = create_forest(25);

    char line[LINE_BUFFER] = {0};
    node *branch = NULL;
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n')
            continue;

        if (isalnum((unsigned char)line[0])) {
            char *vendor_name;
            uint8_t vendor_id = (uint8_t)strtoul(line, &vendor_name, 16);

            line[strcspn(line, "\r\n")] = '\0';

            while (isspace((unsigned char)*vendor_name)) vendor_name++;

            branch = create_node(vendor_id, vendor_name);
            add_tree_to_forest(arm_forest, branch);
        }

        else if (line[0] == '\t') {
            char *device_name;
            uint16_t device_id = (uint16_t)strtoul(line, &device_name, 16);

            line[strcspn(line, "\r\n")] = '\0';

            while (isspace((unsigned char)*device_name)) device_name++;

            device_name[strcspn(device_name, "\n")] = '\0';
            add_child(branch, device_id, device_name);
        }
    }

    fclose(fp);

    return arm_forest;
}

static void hw_arm_lookup_name(forest *arm_forest, char *out_buf, const size_t buf_size) {
    if (!out_buf || buf_size == 0) {
        V_PRINTF("[ERROR] hw_arm_lookup_name: incorrect arguments "
                 "(buf = %p, size = %zu)\n", (void *)out_buf, buf_size);
        return;
    }

    char *endptr;
    uint8_t vendor_id = (uint8_t)strtoul(out_buf, &endptr, 16);
    uint16_t device_id  = (uint16_t)strtoul(endptr, NULL, 16);

    if (arm_forest) {
        node *vendor = find_in_forest(arm_forest, vendor_id);
        node *device = find_in_tree(vendor, device_id);

        if (vendor && device) {
            snprintf(out_buf, buf_size, "%s %s", vendor->name, device->name);
            return;
        } else if (vendor) {
            snprintf(out_buf, buf_size, "%s [0x%04X]", vendor->name, device_id);
            return;
        }
    }

    snprintf(out_buf, buf_size, "Generic ARM CPU [0x%04X 0x%04X]", vendor_id, device_id);
}
#endif

void hw_get_gpu_info(void) {
    gpu_node_t *gpu_list = hw_get_all_gpus();
    if (gpu_list == NULL) {
#if defined(__i386__) || defined(__x86_64__) || defined(__ia64__) || defined(__powerpc64__)
        V_PRINTF("[WARNING] No GPUs found in system (might be VM?)\n");
#endif
        return;
    }

    char info_buf[GPU_BUFFER] = {0};

    forest *pci_forest = hw_pci_forest_create();
    if (pci_forest == NULL) {
        V_PRINTF("[WARNING] Failed to build a PCI forest. Using raw values\n");
    }

    hw_gpu_lookup_names(info_buf, sizeof(info_buf), pci_forest, gpu_list);

    free_gpu_list(gpu_list);

    if (info_buf[0] != '\0') ui_print_info("GPU", info_buf);
}

static gpu_node_t *hw_get_all_gpus(void) {
    const char *prefix = "card";
    size_t prefix_len = strlen("card");

    DIR *dir = opendir(SYS_GPU_DIR);
    if (!dir) {
        V_PRINTF("[ERROR] Failed to open GPU directory %s: %s\n",
                 SYS_GPU_DIR, strerror(errno));
        return NULL;
    }

    gpu_node_t *head = NULL;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, prefix, prefix_len) != 0) continue;

        const char *suffix = entry->d_name + prefix_len;
        if (!util_is_numeric_string(suffix)) continue;

        uint16_t vendor_id = 0, device_id = 0;
        char full_path[PATH_BUFFER] = {0};

        snprintf(full_path, sizeof(full_path), "%s%s/device/vendor", SYS_GPU_DIR, entry->d_name);
        util_read_hex16(full_path, &vendor_id);

        snprintf(full_path, sizeof(full_path), "%s%s/device/device", SYS_GPU_DIR, entry->d_name);
        util_read_hex16(full_path, &device_id);

        head = add_gpu(head, vendor_id, device_id);
    }

    closedir(dir);

    return head;
}

static forest *hw_pci_forest_create(void) {
    const char *paths[] = PCI_IDS_PATHS;

    FILE *fp = NULL;
    for (uint8_t i = 0; paths[i] != NULL; i++) {
        fp = fopen(paths[i], "rb");
        if (fp) break;
    }

    if (!fp) {
        V_PRINTF("[ERROR] Failed to open pci.ids file: %s\n", strerror(errno));
        return NULL;
    }

    forest *pci_forest = create_forest(3000);

    char line[LINE_BUFFER] = {0};
    node *branch = NULL;
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n' || line[1] == '\t') continue;

        if (isalnum((unsigned char)line[0])) {
            char *vendor_name;
            uint16_t vendor_id = (uint16_t)strtoul(line, &vendor_name, 16);

            line[strcspn(line, "\r\n")] = '\0';

            while (*vendor_name == ' ') vendor_name++;

            branch = create_node(vendor_id, vendor_name);
            add_tree_to_forest(pci_forest, branch);
        }

        else if (line[0] == '\t') {
            char *device_name;
            uint16_t device_id = (uint16_t)strtoul(line, &device_name, 16);

            line[strcspn(line, "\r\n")] = '\0';

            while (*device_name == ' ') device_name++;

            device_name[strcspn(device_name, "\n")] = '\0';
            add_child(branch, device_id, device_name);
        }
    }

    fclose(fp);

    return pci_forest;
}

static void hw_gpu_lookup_names(char *out_buf, const size_t buf_size,
                                forest *pci_forest, gpu_node_t *gpu_list) {
    if (!out_buf || buf_size == 0) {
        V_PRINTF("[ERROR] hw_gpu_lookup_names: incorrect arguments "
                 "(buf = %p, size = %zu)\n", (void *)out_buf, buf_size);

        if (pci_forest) destroy_forest(pci_forest);
        return;
    }

    gpu_data_t gpus[MAX_GPUS] = {0};
    uint8_t gpu_count = 0;

    for (gpu_node_t *curr = gpu_list; curr != NULL && gpu_count < MAX_GPUS;
         curr = curr->next) {
        const char *vendor_name = "Generic GPU";
        const char *device_name = "";
        const char *separator = "";

        if (pci_forest) {
            node *vendor = find_in_forest(pci_forest, curr->impl_id);
            node *device = find_in_tree(vendor, curr->part_id);

            if (vendor) {
                vendor_name = vendor->name;
            }

            if (device) {
                device_name = device->name;
                separator = " ";
            }
        }

        size_t offset = strlen(out_buf);
        if (offset >= buf_size) {
            V_PRINTF("[WARNING] hw_gpu_lookup_names: buffer overflow, truncating list\n");
            break;
        }

        snprintf(gpus[gpu_count].model, sizeof(gpus[gpu_count].model), 
                 "%s%s%s [0x%04X]", vendor_name, separator, device_name, curr->part_id);
        gpu_count++;
    }

    const char *gpu_strings[MAX_GPUS] = {0};
    for (uint8_t i = 0; i < gpu_count; i++) {
        gpu_strings[i] = gpus[i].model;
    }

    util_format_duplicate_hardware(gpu_strings, gpu_count, out_buf, buf_size);

    if (pci_forest) destroy_forest(pci_forest);
}

void hw_get_bat_info(void) {
    hw_node_t *list = hw_get_all_batteries();
    if (list == NULL) return;

    char full_path[PATH_BUFFER] = {0};
    size_t base_len = strlen(SYS_BATTERY_DIR);
    memcpy(full_path, SYS_BATTERY_DIR, base_len);

    for (hw_node_t *curr = list; curr != NULL; curr = curr->next) {
        char attr_buf[SMALL_BUFFER] = {0};
        char label_buf[MEDIUM_BUFFER] = {0};
        char info_buf[MEDIUM_BUFFER] = {0};

        size_t name_len = strlen(curr->name);
        memcpy(full_path + base_len, curr->name, name_len);

        size_t file_offset = base_len + name_len;
        full_path[file_offset] = '\0';

        memcpy(full_path + file_offset, "/model_name", sizeof("/model_name"));
        util_read_line(full_path, attr_buf, sizeof(attr_buf));
        snprintf(label_buf, sizeof(label_buf), "Battery (%s)", attr_buf[0] ? attr_buf : "Unknown");

        memcpy(full_path + file_offset, "/capacity", sizeof("/capacity"));
        util_read_line(full_path, attr_buf, sizeof(attr_buf));
        snprintf(info_buf, sizeof(info_buf), "%s%%", attr_buf);

        memcpy(full_path + file_offset, "/status", sizeof("/status"));
        util_read_line(full_path, attr_buf, sizeof(attr_buf));

        size_t current_len = strlen(info_buf);
        snprintf(info_buf + current_len, sizeof(info_buf) - current_len,
                 " (%s)", attr_buf[0] ? attr_buf : "Unknown");

        ui_print_info(label_buf, info_buf);
    }

    free_data_list(list);
}

static hw_node_t *hw_get_all_batteries(void) {
    DIR *dir = opendir(SYS_BATTERY_DIR);
    if (!dir) return NULL;

    hw_node_t *head = NULL;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char type_path[PATH_MAX];
        snprintf(type_path, sizeof(type_path), "%s%s/type", SYS_BATTERY_DIR, entry->d_name);

        FILE *fp = fopen(type_path, "r");
        if (!fp) continue;

        char type[SMALL_BUFFER];
        if (fgets(type, sizeof(type), fp) && strncmp(type, "Battery", 7) == 0) {
            head = add_element(head, entry->d_name);
        }

        fclose(fp);
    }

    closedir(dir);

    return head;
}

static gpu_node_t *add_gpu(gpu_node_t *head, const uint16_t impl_id, const uint16_t part_id) {
    gpu_node_t *new_node = malloc(sizeof(gpu_node_t));
    if (!new_node) return head;

    memset(new_node, 0, sizeof(gpu_node_t));

    new_node->impl_id = impl_id;
    new_node->part_id = part_id;
    new_node->next = head;

    return new_node;
}

static void free_gpu_list(gpu_node_t *head) {
    while (head) {
        gpu_node_t *temp = head;
        head = head->next;
        free(temp);
    }
}

static hw_node_t *add_element(hw_node_t *head, const char *name) {
    hw_node_t *new_node = malloc(sizeof(hw_node_t));
    if (!new_node) return head;

    memset(new_node, 0, sizeof(hw_node_t));

    size_t name_len = strlen(name);
    size_t to_copy = (name_len >= sizeof(new_node->name)) 
                     ? sizeof(new_node->name) - 1 : name_len;

    memcpy(new_node->name, name, to_copy);
    new_node->name[to_copy] = '\0';

    new_node->next = head;
    return new_node;
}

static void free_data_list(hw_node_t *head) {
    while (head) {
        hw_node_t *temp = head;
        head = head->next;
        free(temp);
    }
}
