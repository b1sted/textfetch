/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

#include <sys/mount.h>
#include <sys/sysctl.h>

#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach_types.h>
#include <mach/vm_statistics.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <TargetConditionals.h>

#include "defs.h"
#include "hashtable.h"
#include "sys_utils.h"
#include "ui.h"

#include "pal/hardware_os.h"

/**
 * Compatibility shim for kIOMainPortDefault.
 *
 * macOS 12.0 renamed kIOMasterPortDefault to kIOMainPortDefault.
 * This definition ensures the code compiles on older SDKs (like
 * Mavericks/Mojave) while avoiding deprecation warnings on modern macOS
 * versions.
 */
#ifndef kIOMainPortDefault
#define kIOMainPortDefault ((mach_port_t)0)
#endif

#if TARGET_CPU_ARM64
/**
 * Static lookup entry for mapping SoC brand strings to peak P-core frequencies.
 *
 * Used as a stable alternative to querying undocumented IOKit PMGR nodes,
 * which are subject to format changes across different Apple Silicon
 * generations.
 */
typedef struct {
    double p_frequency; /* Peak Performance-core clock speed in GHz */
    double e_frequency; /* Peak Efficiency-core clock speed in GHz */
    const char *model;
} dvfs_entry_t;
#endif

/* System power status bitmask flags indicating charging state. */
typedef enum {
    FLAG_AC       = 1 << 0,
    FLAG_CHARGING = 1 << 1,
    FLAG_FULL     = 1 << 2
} power_flags;

/**
 * Retrieves the total count of physical CPU packages via sysctl.
 *
 * @param node Pointer to the CPU info structure.
 */
static void hw_get_cpu_package(cpu_info_t *node);

/**
 * Retrieves the CPU model name string via sysctl.
 *
 * @param node Pointer to the CPU info structure.
 */
static void hw_get_cpu_model(cpu_info_t *node);

/**
 * Retrieves the physical core counts, separated by P and E cores on ARM64.
 *
 * @param node Pointer to the CPU info structure.
 */
static void hw_get_cpu_cores(cpu_info_t *node);

/**
 * Retrieves the maximum CPU frequency (hardcoded lookup for ARM64, sysctl for x86).
 *
 * @param node Pointer to the CPU info structure.
 */
static void hw_get_cpu_freq(cpu_info_t *node);

/**
 * Queries IOKit for GPUs handled by the IOAccelerator subsystem (typically Apple GPUs).
 *
 * @param out_buf Buffer to format the found GPUs into.
 * @param buf_size Size of the buffer.
 */
static void hw_get_gpu_accelerator(char *out_buf, const size_t buf_size);

/**
 * Queries IOKit for discrete GPUs using the IOPCIDevice subsystem class codes.
 *
 * @param out_buf Buffer to format the found GPUs into.
 * @param buf_size Size of the buffer.
 */
static void hw_get_gpu_pci(char *out_buf, const size_t buf_size);

/**
 * Evaluates the current RAM capacity and usage using Mach host statistics.
 *
 * @param flags Bitmask storing the detected memory types.
 * @param node Pointer to the memory info block.
 */
static void hw_get_ram_info(mem_flags_t *flags, mem_info_t *node);

/**
 * Evaluates swap usage using sysctl vm.swapusage.
 *
 * @param flags Bitmask storing the detected memory types.
 * @param node Pointer to the memory info block.
 */
static void hw_get_swap_info(mem_flags_t *flags, mem_info_t *node);

/**
 * Calculates current battery percentage from an IOPowerSources dictionary.
 *
 * @param power_source Pointer to the battery details dictionary.
 * @return Remaining battery capacity in percent.
 */
static uint8_t hw_get_bat_percentage(const CFDictionaryRef power_source);

/**
 * Resolves the string representing the charging state of the battery.
 *
 * @param power_source Pointer to the battery details dictionary.
 * @param battery_percentage The computed battery percentage.
 * @return Constant string representing the charging state.
 */
static const char *hw_get_bat_status(const CFDictionaryRef power_source,
                                     uint8_t battery_percentage);

/**
 * Creates an IOKit iterator matching all services of a given class.
 *
 * @param class_name The IOKit class name to match (e.g., "IOAccelerator").
 * @return An io_iterator_t on success, or IO_OBJECT_NULL on failure.
 */
static io_iterator_t get_matching_iterator(const char *class_name);

void hw_get_cpu_info(void) {
    cpu_info_t node;
    memset(&node, 0, sizeof(cpu_info_t));

    hw_get_cpu_package(&node);
    hw_get_cpu_model(&node);
    hw_get_cpu_cores(&node);
    hw_get_cpu_freq(&node);

    char cpu_info[LINE_BUFFER] = {0};
    snprintf(cpu_info, sizeof(cpu_info), "%s%s%s", node.model, node.cores, node.frequency);

    ui_print_info("CPU", cpu_info);
}

static void hw_get_cpu_package(cpu_info_t *node) {
    uint32_t packages = 0;
    size_t packages_size = sizeof(packages);

    if (sysctlbyname("hw.packages", &packages, &packages_size, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(hw.packages) failed: %s\n", strerror(errno));
    }

    node->packages = (packages) ? (uint8_t)packages : 1;
}

static void hw_get_cpu_model(cpu_info_t *node) {
    size_t model_size = sizeof(node->model);

    if (sysctlbyname("machdep.cpu.brand_string", node->model, &model_size, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(machdep.cpu.brand_string) failed: %s\n", strerror(errno));
    }

    if (node->model[0] == '\0') {
        snprintf(node->model, model_size, "Unknown");
        return;
    }

    if (node->packages > 1) {
        char prefix[TINY_BUFFER] = {0};
        snprintf(prefix, sizeof(prefix), "%" PRIu32 " x ", node->packages);
        size_t prefix_len = strlen(prefix);

        memmove(node->model + prefix_len, node->model, strlen(prefix) + 1);
        memcpy(node->model, prefix, prefix_len);
    }

    size_t len = strlen(node->model);
    while (len > 0 && isspace((unsigned char)node->model[len - 1])) {
        node->model[--len] = '\0';
    }
}

static void hw_get_cpu_cores(cpu_info_t *node) {
    uint64_t common_cores = 0;
    size_t common_size = sizeof(common_cores);

    if (sysctlbyname("hw.physicalcpu", &common_cores, &common_size, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(hw.physicalcpu) failed: %s\n", strerror(errno));
    }

    size_t core_size = sizeof(node->cores);
    snprintf(node->cores, core_size, " (%" PRIu64 ")", common_cores);

#if TARGET_CPU_ARM64
    uint64_t p_cores = 0;
    size_t p_size = sizeof(p_cores);

    if (sysctlbyname("hw.physicalcpu", p_cores, &p_size, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(hw.physicalcpu) failed: %s\n", strerror(errno));
    }

    uint64_t e_cores = 0;
    size_t e_size = sizeof(e_cores);

    if (sysctlbyname("hw.physicalcpu", e_cores, &e_size, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(hw.physicalcpu) failed: %s\n", strerror(errno));
    }

    size_t current_len = strlen(node->cores) - 1;
    snprintf(node->cores + current_len, core_size - current_len,
             "-core: %" PRIu64 "P + %" PRIu64 "E)", p_cores, e_cores);
#endif
}

#if TARGET_CPU_ARM64
static void hw_get_cpu_freq(cpu_info_t *node) {
    if (model == NULL) return 0.0;

    /**
     * Friendship ended with IOKIT
     * Now HARDCODED TABLE is my best friend
     *
     * PMGR and IOKit nodes for frequency scaling are undocumented and change
     * frequently across macOS versions. A static lookup table indexed by SoC
     * brand strings provides a more reliable and stable frequency report.
     *
     * Note: Search order is crucial (e.g., "M1 Max" must be checked before "M1").
     */
    static const dvfs_entry_t m_series[] = {
        {3.23, 2.06, "M1 Ultra"},
        {3.23, 2.06, "M1 Max"},
        {3.23, 2.06, "M1 Pro"},
        {3.20, 2.06, "M1"},
        {3.70, 2.42, "M2 Ultra"},
        {3.69, 2.42, "M2 Max"},
        {3.50, 2.42, "M2 Pro"},
        {3.50, 2.42, "M2"},
        {4.05, 2.75, "M3 Ultra"},
        {4.05, 2.75, "M3 Max"},
        {4.05, 2.75, "M3 Pro"},
        {4.05, 2.75, "M3"},
        {4.51, 2.89, "M4 Max"},
        {4.51, 2.89, "M4 Pro"},
        {4.41, 2.89, "M4"},
        {4.61, 3.10, "M5"},
        {0, 0, NULL}
    };

    for (uint8_t i = 0; m_series[i].model != NULL; i++) {
        if (strstr(node->model, m_series[i].model) != NULL) {
            snprintf(node->frequency, sizeof(node->frequency),
                     " @ %.2f / %.2f GHz", m_series[i].p_frequency,
                     m_series[i].e_frequency);
            return;
        }
    }
}
#else
static void hw_get_cpu_freq(cpu_info_t *node) {
    uint64_t hz = 0;
    size_t size = sizeof(hz);

    /* Using nominal maximum frequency (advertised clock speed) */
    if (sysctlbyname("hw.cpufrequency_max", &hz, &size, NULL, 0) == -1) {
        V_PRINTF("[ERROR] sysctlbyname(hw.cpufrequency_max) failed: %s\n", strerror(errno));
    }

    snprintf(node->frequency, sizeof(node->frequency), " @ %.2f GHz", (double)hz / HZ_PER_GHZ);
}
#endif

void hw_get_gpu_info(void) {
    char gpu_buf[GPU_BUFFER] = {0};
    size_t gpu_size = sizeof(gpu_buf);

    hw_get_gpu_accelerator(gpu_buf, gpu_size);

    if (gpu_buf[0] == '\0') hw_get_gpu_pci(gpu_buf, gpu_size);

    if (gpu_buf[0] != '\0') ui_print_info("GPU", gpu_buf);
}

static void hw_get_gpu_accelerator(char *out_buf, const size_t buf_size) {
    io_iterator_t iterator = get_matching_iterator("IOAccelerator");
    if (iterator != IO_OBJECT_NULL) {
        io_service_t service;
        while ((service = IOIteratorNext(iterator))) {
            CFTypeRef model = IORegistryEntryCreateCFProperty(service, CFSTR("model"), kCFAllocatorDefault, 0);

            if (!model) {
                io_registry_entry_t parent;
                if (IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent) == kIOReturnSuccess) {
                    model = IORegistryEntryCreateCFProperty(parent, CFSTR("model"), kCFAllocatorDefault, 0);
                    IOObjectRelease(parent);
                }
            }

            if (model) {
                char name[MEDIUM_BUFFER] = {0};

                if (CFGetTypeID(model) == CFDataGetTypeID()) {
                    snprintf(name, sizeof(name), "%s", (const char *)CFDataGetBytePtr((CFDataRef)model));
                } else if (CFGetTypeID(model) == CFStringGetTypeID()) {
                    CFStringGetCString((CFStringRef)model, name, sizeof(name), kCFStringEncodingUTF8);
                }

                if (name[0] != '\0') {
                    if (out_buf[0] != '\0') strlcat(out_buf, "\n     ", buf_size);
                    strlcat(out_buf, name, buf_size);
                }

                CFRelease(model);
            }

            IOObjectRelease(service);
        }

        IOObjectRelease(iterator);
    }
}

static void hw_get_gpu_pci(char *out_buf, const size_t buf_size) {
    io_iterator_t iterator = get_matching_iterator("IOPCIDevice");
    if (iterator != IO_OBJECT_NULL) {
        io_registry_entry_t entry;
        while ((entry = IOIteratorNext(iterator))) {
            CFTypeRef class_code = IORegistryEntryCreateCFProperty(entry, CFSTR("class-code"), kCFAllocatorDefault, 0);
            if (class_code) {
                const uint8_t *code = CFDataGetBytePtr((CFDataRef)class_code);
                if (code[2] == 0x03) {
                    CFTypeRef model = IORegistryEntryCreateCFProperty(entry, CFSTR("model"), kCFAllocatorDefault, 0);
                    if (model) {
                        const char *m_ptr = (const char *)CFDataGetBytePtr((CFDataRef)model);

                        if (out_buf[0] != '\0') strlcat(out_buf, "\n     ", buf_size);
                        strlcat(out_buf, m_ptr, buf_size);
                        CFRelease(model);
                    }
                }

                CFRelease(class_code);
            }

            IOObjectRelease(entry);
        }

        IOObjectRelease(iterator);
    }
}

void hw_get_mem_info(void) {
    mem_info_t node;
    memset(&node, 0, sizeof(mem_info_t));

    mem_flags_t flags = 0;

    hw_get_ram_info(&flags, &node);
    hw_get_swap_info(&flags, &node);
    hw_print_mem_info(flags, &node);
}

static void hw_get_ram_info(mem_flags_t *flags, mem_info_t *node) {
    size_t len = sizeof(node->ram_size);

    if (sysctlbyname("hw.memsize", &node->ram_size, &len, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(hw.memsize) failed: %s\n", strerror(errno));
        return;
    }

    if (node->ram_size == 0) return;

    *flags |= MEM_RAM;

    mach_port_t host = mach_host_self();
    vm_statistics64_data_t vm;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_size_t pg;

    if (host_page_size(host, &pg) != KERN_SUCCESS) {
        V_PRINTF("[ERROR] Mach host_page_size() failed\n");
        return;
    }

    if (host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vm, &count) != KERN_SUCCESS) {
        V_PRINTF("[ERROR] Mach host_statistics64() failed\n");
        return;
    }

    int64_t wired = (int64_t)vm.wire_count * pg;

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_9
    int64_t active = (int64_t)vm.active_count * pg;
    int64_t inactive = (int64_t)vm.inactive_count * pg;
    node->ram_used = active + inactive + wired;
#else
    int64_t app = (int64_t)(vm.internal_page_count - vm.purgeable_count) * pg;
    int64_t compressed = (int64_t)vm.compressor_page_count * pg;
    node->ram_used = app + wired + compressed;
#endif
}

static void hw_get_swap_info(mem_flags_t *flags, mem_info_t *node) {
    struct xsw_usage xsw;
    size_t xsw_len = sizeof(struct xsw_usage);

    if (sysctlbyname("vm.swapusage", &xsw, &xsw_len, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(vm.swapusage) failed: %s\n", strerror(errno));
        return;
    }

    if (xsw.xsu_total == 0) return;

    *flags |= MEM_SWAP;

    node->swap_size = xsw.xsu_total;
    node->swap_used = xsw.xsu_used;
}

void hw_get_drives_info(void) {
    int n = getfsstat(NULL, 0, MNT_WAIT);
    if (n <= 0) {
        V_PRINTF("[ERROR] getfsstat() size query failed: %s\n", strerror(errno));
        return;
    }

    struct statfs *st = malloc(sizeof(struct statfs) * n);
    if (!st) {
        V_PRINTF("[ERROR] malloc(%zu) for statfs failed: %s\n",
                 sizeof(struct statfs) * n, strerror(errno));
        return;
    }

    n = getfsstat(st, (int)(sizeof(struct statfs) * n), MNT_NOWAIT);
    if (n <= 0) {
        V_PRINTF("[ERROR] getfsstat() data fetch failed: %s\n", strerror(errno));
        free(st);
        return;
    }

    string_set_t *outputted_disks = strset_create(INITIAL_CAPACITY);

    for (int i = 0; i < n; i++) {
        if (strcmp(st[i].f_mntonname, "/") != 0 &&
            strncmp(st[i].f_mntonname, "/Volumes/", 9) != 0) continue;

        if (strset_contains(outputted_disks, st[i].f_mntfromname)) continue;

        strset_add(outputted_disks, st[i].f_mntfromname);

        uint64_t total = (uint64_t)st[i].f_blocks * st[i].f_bsize;
        uint64_t used = total - ((uint64_t)st[i].f_bfree * st[i].f_bsize);

        char label[LINE_BUFFER], info[LINE_BUFFER];
        snprintf(label, sizeof(label), "Disk (%s)", st[i].f_mntonname);
        util_format_size((double)total, (double)used, info, sizeof(info), UNIT_B);

        size_t cur_len = strlen(info);
        snprintf(info + cur_len, LINE_BUFFER - cur_len, " [%s]", st[i].f_fstypename);
        ui_print_info(label, info);
    }

    strset_destroy(outputted_disks);
    free(st);
}

void hw_get_bat_info(void) {
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    if (!info) {
        V_PRINTF("[ERROR] IOPSCopyPowerSourcesInfo() failed (no power sources found)\n");
        return;
    }

    CFArrayRef list = IOPSCopyPowerSourcesList(info);
    if (!list || CFArrayGetCount(list) == 0) {
        if (list) CFRelease(list);
        CFRelease(info);
        return;
    }

    CFDictionaryRef dict = IOPSGetPowerSourceDescription(info,
                                                         CFArrayGetValueAtIndex(list, 0));

    char model[SMALL_BUFFER] = "Unknown";
    CFStringRef model_ref = CFDictionaryGetValue(dict, CFSTR("DeviceName"));

    if (!model_ref) {
        V_PRINTF("[WARNING] Battery specific name not found; using fallback: %s\n", kIOPSNameKey);
        model_ref = CFDictionaryGetValue(dict, CFSTR(kIOPSNameKey));
    }

    if (model_ref && CFGetTypeID(model_ref) == CFStringGetTypeID()) {
        CFStringGetCString(model_ref, model, sizeof(model), kCFStringEncodingUTF8);
    }

    char health[SMALL_BUFFER] = "Unknown";
    CFStringRef health_ref = CFDictionaryGetValue(dict, CFSTR(kIOPSBatteryHealthKey));
    if (health_ref) {
        CFStringGetCString(health_ref, health, sizeof(health), kCFStringEncodingUTF8);
    }

    uint8_t pct = hw_get_bat_percentage(dict);
    const char *status = hw_get_bat_status(dict, pct);

    char label_buf[LINE_BUFFER] = {0};
    char info_buf[MEDIUM_BUFFER] = {0};

    snprintf(label_buf, sizeof(label_buf), "Battery (%s)", model);
    snprintf(info_buf, sizeof(info_buf), "%u%% (%s, Health: %s)", pct, status, health);

    CFRelease(list);
    CFRelease(info);
}

static uint8_t hw_get_bat_percentage(const CFDictionaryRef power_source) {
    int cur_cap = 0, max_cap = 0;

    CFNumberRef cur_ref = CFDictionaryGetValue(power_source, CFSTR(kIOPSCurrentCapacityKey));
    CFNumberRef max_ref = CFDictionaryGetValue(power_source, CFSTR(kIOPSMaxCapacityKey));

    if (cur_ref) CFNumberGetValue(cur_ref, kCFNumberIntType, &cur_cap);
    if (max_ref) CFNumberGetValue(max_ref, kCFNumberIntType, &max_cap);

    /* Check max_cap > 0 to avoid division by zero if battery is reporting error */
    return (max_cap > 0) ? (uint8_t)((double)cur_cap / max_cap * 100) : 0;
}

static const char *hw_get_bat_status(const CFDictionaryRef power_source,
                                     uint8_t battery_percentage) {
    uint8_t status = 0;
    CFStringRef state = CFDictionaryGetValue(power_source, CFSTR(kIOPSPowerSourceStateKey));
    CFBooleanRef charging = CFDictionaryGetValue(power_source, CFSTR(kIOPSIsChargingKey));

    if (state &&
        CFStringCompare(state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo) {
        status |= FLAG_AC;
    }
    if (charging && CFBooleanGetValue(charging))
        status |= FLAG_CHARGING;
    if (battery_percentage >= 100)
        status |= FLAG_FULL;

    if (status & FLAG_FULL && status & FLAG_AC)
        return "Full";
    if (status & FLAG_CHARGING)
        return "Charging";
    if (status & FLAG_AC)
        return "Not Charging";
    return "Discharging";
}

static io_iterator_t get_matching_iterator(const char *class_name) {
    CFMutableDictionaryRef matching_dict = IOServiceMatching(class_name);
    if (!matching_dict) return IO_OBJECT_NULL;

    io_iterator_t iterator;
    if (IOServiceGetMatchingServices(kIOMainPortDefault,
                                     matching_dict, &iterator) != kIOReturnSuccess) {
        V_PRINTF("[ERROR] IOServiceGetMatchingServices failed to find %s\n", class_name);
        return IO_OBJECT_NULL;
    }

    return iterator;
}
