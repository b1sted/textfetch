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

#include "binary_trees.h"
#include "bitset.h"
#include "defs.h"
#include "sys_utils.h"
#include "ui.h"

#include "pal/hardware_os.h"

typedef struct hw_node {
    char name[SMALL_BUFFER];
    struct hw_node *next;
} hw_node_t;

typedef struct gpu_node {
    uint16_t impl_id;
    uint16_t part_id;
    struct gpu_node *next;
} gpu_node_t;

static gpu_node_t *hw_get_all_gpus(void);
static forest* hw_pci_forest_create(void);
static void hw_gpu_lookup_names(char *out_buf, const size_t buf_size,
                                forest *pci_forest, gpu_node_t *gpu_list);

static gpu_node_t *add_gpu(gpu_node_t *head, const uint16_t impl_id, const uint16_t part_id);
static void free_gpu_list(gpu_node_t *head);

static hw_node_t *hw_get_all_batteries(void);

static hw_node_t *add_element(hw_node_t *head, const char *name);
static void free_data_list(hw_node_t *head);

#if defined(__x86_64__) || defined(__i386__)
void hw_get_cpu_model(cpu_info_t *node) {
    unsigned int regs[4];
    char *write_cursor = node->model;

    for (unsigned int leaf_id = 0x80000002; leaf_id <= 0x80000004; leaf_id++) {
        if (__get_cpuid(leaf_id, &regs[0], &regs[1], &regs[2], &regs[3]) == 0) {
            return;
        }

        memcpy(write_cursor, regs, sizeof(regs));
        write_cursor += sizeof(regs);
    }

    *write_cursor = '\0';

    size_t len = strlen(node->model);

    while (len > 0 && isspace((unsigned char)node->model[len - 1])) {
        len--;
        node->model[len] = '\0';
    }
}
#else
void hw_get_cpu_model(cpu_info_t *node) {
#if defined(__aarch64__) || defined(__arm64__)
    char midr_str[TINY_BUFFER] = {0};
    const char *path = "/sys/devices/system/cpu/cpu0/regs/identification/midr_el1";

    uint32_t midr = 0;
    if (util_read_hex(path, &midr)) {
        uint8_t implementer = (midr >> 24) & 0xFF;
        uint16_t partnum = (midr >> 4) & 0xFFF;

        /* TODO: write a parser for ARM64 CPUs */
        const char *vendor;
        switch (implementer) {
            case 0x41: vendor = "ARM"; break;
            case 0x51: vendor = "Qualcomm"; break;
            case 0x61: vendor = "Apple"; break;
            case 0x42: vendor = "Broadcom"; break;
            case 0x43: vendor = "Cavium"; break;
            case 0x4E: vendor = "NVIDIA"; break;
            default:   vendor = "Generic ARM"; break;
        }

        snprintf(node->model, sizeof(node->model), "%s [%" PRIu16 "]", vendor, partnum);
        return;
    }
#else
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        V_PRINTF("Error: failed to open CPU info file: %s\n", strerror(errno));
        return;
    }

    char line[LINE_BUFFER];
    while (fgets(line, sizeof(line), fp)) {
        char *delim = strchr(line, ':');
        if (!delim) continue;

        *delim = 0;
        char *key = line;
        char *value = delim + 1;

        value[strcspn(value, "\n")] = 0;

        if (value[0] == ' ') value++;

        if (strncmp(key, "model name", 10) == 0) {
            snprintf(node->model, sizeof(node->model), "%s", value);
            break;
        }
    }

    fclose(fp);
}
#endif
#endif

void hw_get_cpu_cores(cpu_info_t *node) {
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
            V_PRINTF("Error: failed to open CPU core ID file: %s\n", strerror(errno));
            break;
        }

        if (fscanf(core_file, "%d", &core_id) != 1) {
            V_PRINTF("Error: failed to parse numeric value from sysfs\n");
        }

        fclose(core_file);

        if (!set_contains(unique_cores, core_id)) set_add(unique_cores, core_id);
    }

    uint32_t cores_count = count_set_bits(unique_cores, SET_SIZE);
    snprintf(node->cores, sizeof(node->cores), " (%" PRIu32 ")", cores_count);
}

void hw_get_cpu_freq(cpu_info_t *node) {
    uint32_t freq_khz = 0;
    util_read_uint32("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", &freq_khz);

    double frequency = (double)freq_khz / KHZ_PER_GHZ;
    snprintf(node->frequency, sizeof(node->frequency), " @ %.03f GHz", frequency);
}

void hw_get_gpu_info(void) {
    gpu_node_t *gpu_list = hw_get_all_gpus();
    if (gpu_list == NULL) {
#if defined(__i386__) || defined(__x86_64__) || defined(__ia64__) || defined(__powerpc64__)
        V_PRINTF("[WARNING] No GPU's found in system (might be VM?)\n");
#endif
        return;
    }

    char info_buf[GPU_BUFFER] = {0};

    forest *pci_forest = hw_pci_forest_create();
    if (pci_forest == NULL) {
        V_PRINTF("[WARNING] Fail to build a PCI forest. Using a raw values\n");
    }

    hw_gpu_lookup_names(info_buf, sizeof(info_buf), pci_forest, gpu_list);

    free_gpu_list(gpu_list);

    if (info_buf[0] != '\0') ui_print_info("GPU", info_buf);
}

static gpu_node_t *hw_get_all_gpus(void) {
    const char *gpu_directory = "/sys/class/drm/";
    const char *prefix = "card";
    size_t prefix_len = strlen("card");

    DIR *dir = opendir(gpu_directory);
    if (!dir) {
        V_PRINTF("Error: failed to open GPU directory %s: %s\n", 
                 gpu_directory, strerror(errno));
        return NULL;
    }

    gpu_node_t *head = NULL;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, prefix, prefix_len) != 0) continue;

        const char *suffix = entry->d_name + prefix_len;
        if (*suffix == '\0') continue;

        const char *cursor = suffix;
        bool valid_name = true;
        while (*cursor) {
            if (!isdigit(*cursor)) {
                valid_name = false;
                break;
            }

            cursor++;
        }

        if (!valid_name) continue;

        uint16_t vendor_id = 0, device_id = 0;
        char full_path[PATH_BUFFER] = {0};

        snprintf(full_path, sizeof(full_path), "%s%s/device/vendor", gpu_directory, entry->d_name);
        util_read_hex16(full_path, &vendor_id);

        snprintf(full_path, sizeof(full_path), "%s%s/device/device", gpu_directory, entry->d_name);
        util_read_hex16(full_path, &device_id);

        head = add_gpu(head, vendor_id, device_id);
    }

    closedir(dir);

    return head;
}

static forest* hw_pci_forest_create(void) {
    const char *search_paths[] = {
        "/var/lib/pciutils/pci.ids", "/usr/share/hwdata/pci.ids",
        "/usr/share/misc/pci.ids", "/usr/share/pci.ids",
        "/opt/homebrew/share/pci.ids", "pci.ids", NULL
    };

    FILE *fp = NULL;
    for (uint8_t i = 0; search_paths[i] != NULL; i++) {
        fp = fopen(search_paths[i], "rb");
        if (fp) break;
    }

    if (!fp) {
        V_PRINTF("[ERROR] Fail to open pci.ids file: %s\n", strerror(errno));
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
        V_PRINTF("[ERROR] hw_gpu_lookup_names: incorrect arguments (buf = %p, size = %zu)\n", 
                 (void*)out_buf, buf_size);
        
        if (pci_forest) destroy_forest(pci_forest);
        return;
    }

    for (gpu_node_t *curr = gpu_list; curr != NULL; curr = curr->next) {
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

        snprintf(out_buf + offset, buf_size - offset,
                 "%s%s%s%s [0x%04X]", (out_buf[0] == '\0') ? "" : "\n     ",
                 vendor_name, separator, device_name, curr->part_id);
    }

    if (pci_forest) destroy_forest(pci_forest);
}

void hw_get_bat_info(void) {
    hw_node_t *list = hw_get_all_batteries();
    if (list == NULL) return;

    const char *base_path = "/sys/class/power_supply/";
    size_t base_len = strlen(base_path);

    char full_path[PATH_BUFFER] = {0};
    memcpy(full_path, base_path, base_len);

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
    const char *power_path = "/sys/class/power_supply/";
    DIR *dir = opendir(power_path);
    if (!dir) return NULL;

    hw_node_t *head = NULL;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char type_path[PATH_MAX];
        snprintf(type_path, sizeof(type_path), "%s%s/type", power_path, entry->d_name);

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
