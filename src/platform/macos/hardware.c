/* SPDX-License-Identifier: MIT */

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mount.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach_types.h>
#include <mach/vm_statistics.h>

#include <TargetConditionals.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>

#include "hashtable.h"
#include "hardware.h"
#include "ui.h"

/**
 * Compatibility shim for kIOMainPortDefault.
 * 
 * macOS 12.0 renamed kIOMasterPortDefault to kIOMainPortDefault.
 * This definition ensures the code compiles on older SDKs (like Mavericks/Mojave)
 * while avoiding deprecation warnings on modern macOS versions.
 */
#ifndef kIOMainPortDefault
#define kIOMainPortDefault ((mach_port_t)0)
#endif

#if TARGET_CPU_ARM64
/**
 * Static lookup entry for mapping SoC brand strings to peak P-core frequencies.
 * 
 * Used as a stable alternative to querying undocumented IOKit PMGR nodes,
 * which are subject to format changes across different Apple Silicon generations.
 */
typedef struct {
    double p_frequency; /* Peak Performance-core clock speed in GHz. */
    const char *model;
} dvfs_entry_t;
#endif

/**
 * System power status bitmask flags.
 */
typedef enum {
    FLAG_AC       = 1 << 0,
    FLAG_CHARGING = 1 << 1,
    FLAG_FULL     = 1 << 2
} power_flags;

/**
 * CPU Information Helpers
 * Functions to query sysctl nodes for hardware identifiers, 
 * physical core counts, and maximum clock speeds.
 */

/**
 * Aggregates CPU model name, physical core count, and max frequency into a 
 * formatted string (e.g., "Apple M1 Max (10) @ 3.23 GHz").
 * 
 * @param out_buf  The destination buffer.
 * @param buf_size The size of the destination buffer.
 */
static void hw_get_cpu_info(char *out_buf, const size_t buf_size);

/**
 * Retrieves the CPU brand string using sysctl (machdep.cpu.brand_string).
 * Trims trailing whitespace often present in Apple Silicon brand strings.
 * 
 * @param model_buf The buffer to store the model name.
 * @param buf_size  The size of the buffer.
 */
static void hw_get_cpu_model(char *model_buf, size_t buf_size);

/**
 * Gets the number of physical (performance + efficiency) CPU cores.
 * Excludes logical threads (Hyper-Threading) on Intel systems.
 * 
 * @param core_count Pointer to store the count.
 */
static void hw_get_cpu_cores(uint32_t *core_count);

/**
 * Retrieves the maximum CPU frequency in GHz.
 * 
 * On ARM64 (Apple Silicon): Determined via a static lookup table of M-series
 * SoC specifications for better stability across macOS versions.
 * On x86_64: Reads the nominal maximum (advertised) frequency via sysctl.
 */
static double hw_get_cpu_freq(const char *model);

/**
 * GPU Detection Helpers
 * - On Intel (x86_64): Scans the I/O Registry for PCI display controllers.
 * - On Apple Silicon (ARM64): The GPU name is extracted from the SoC brand string
 *   within the main printing logic, as it is an integrated part of the M-series chip.
 */

#if !TARGET_CPU_ARM64
/**
 * Retrieves GPU model names from the I/O Registry for Intel-based Macs.
 * 
 * Scans 'IOPCIDevice' nodes specifically for Display Controllers (class-code 03xxxxxx).
 * This identifies both integrated Intel graphics and discrete AMD/NVIDIA GPUs,
 * formatting them into a multi-line string if multiple GPUs are present.
 * 
 * @param out_buf  The destination buffer.
 * @param buf_size The size of the destination buffer.
 */
static void hw_get_gpu_info(char *out_buf, const size_t buf_size);
#endif

/**
 * Memory and Storage Helpers
 * Uses Mach VM statistics for RAM and statfs for disk usage.
 */

/**
 * Calculates RAM usage based on the macOS memory management model.
 * 
 * Modern logic (10.9+) replicates Activity Monitor:
 * Used = (Internal - Purgeable) + Wired + Compressed.
 * This excludes 'Cached Files' and 'Free' memory to show actual memory pressure.
 * 
 * @param out_buf  Buffer for formatted RAM info.
 * @param buf_size Size of the buffer.
 */
static void hw_get_ram_info(char *out_buf, const size_t buf_size);

/**
 * Retrieves virtual memory swap usage via the vm.swapusage sysctl node.
 * 
 * @param out_buf  Buffer for formatted Swap info.
 * @param buf_size Size of the buffer.
 */
static void hw_get_swap_info(char *out_buf, const size_t buf_size);

/**
 * Iterates through mounted file systems. Filters results to show only the
 * root ("/") and user-visible volumes ("/Volumes/ *"), ignoring system-reserved 
 * or firmware partitions.
 */
static void hw_scan_and_print_disks(void);

/**
 * Power Supply Helpers
 * Uses the IOKit IOPowerSources API to monitor battery status and health.
 */

/**
 * Collects battery hardware identification, health state, and charging status.
 * 
 * @param label_buf Buffer for the hardware model label (e.g., "Battery (A2141)").
 * @param info_buf  Buffer for status (Percentage, Charging/Full, Health).
 * @param buf_size  Size of the buffers.
 */
static void hw_get_bat_info(char *label_buf, char *info_buf, const size_t buf_size);

/**
 * Calculates current battery percentage based on capacity reports.
 * 
 * @param power_source CFDictionary containing power source properties.
 * @return The calculated percentage (0-100).
 */
static uint8_t hw_get_bat_percentage(const CFDictionaryRef power_source);

/**
 * Maps power source state and capacity to a human-readable status string.
 * Handles edge cases like "Not Charging" when AC is connected but battery is full.
 * 
 * @param power_source       CFDictionary containing power source properties.
 * @param battery_percentage Current percentage to determine "Full" state.
 * @return A constant string: "Charging", "Discharging", "Full", or "Not Charging".
 */
static const char* hw_get_bat_status(const CFDictionaryRef power_source, uint8_t battery_percentage);

/**
 * Utility: Human-readable byte formatting.
 * 
 * Dynamically scales byte values to KiB, MiB, GiB, etc., using a 1024-based 
 * binary prefix (IEC standard units).
 * 
 * @param used_size  The used size in bytes.
 * @param total_size The total size in bytes.
 * @param out_buf    Destination buffer.
 * @param buf_size   Size of the destination buffer.
 */
static void format_bytes(double used_size, double total_size, char *out_buf, const size_t buf_size);

void hardware_print_info(void) {
    char cpu_buf[LINE_BUFFER] = "Unknown";
    hw_get_cpu_info(cpu_buf, LINE_BUFFER);
    ui_print_info("CPU", cpu_buf);

    char gpu_buf[LINE_BUFFER] = "Unknown";
#if TARGET_CPU_ARM64
    /* 
     * On Apple Silicon, GPU info is typically part of the SoC brand string.
     * Example: "Apple M2 Max (12) @ 3.50 GHz" -> Extract "Apple M2 Max".
     * We truncate at the first parenthesis and trim trailing spaces.
     */

    char *delim = strchr(cpu_buf, '(');
    if (delim) {
        size_t count = delim - cpu_buf;
        memcpy(gpu_buf, cpu_buf, count);
        gpu_buf[count] = '\0';

        if (count > 0 && gpu_buf[count - 1] == ' ') gpu_buf[count - 1] = '\0';
    }
#else
    hw_get_gpu_info(gpu_buf, LINE_BUFFER);
#endif
    ui_print_info("GPU", gpu_buf);

    char ram_buf[LINE_BUFFER] = "- / -";
    hw_get_ram_info(ram_buf, LINE_BUFFER);
    ui_print_info("RAM", ram_buf);

    char swap_buf[LINE_BUFFER] = "";
    hw_get_swap_info(swap_buf, LINE_BUFFER);
    if (strlen(swap_buf) > 0) {
        ui_print_info("Swap", swap_buf);
    }
    
    hw_scan_and_print_disks();

    char bat_label[LINE_BUFFER] = "";
    char bat_buf[LINE_BUFFER] = "";
    hw_get_bat_info(bat_label, bat_buf, LINE_BUFFER);
    if (strlen(bat_label) > 0) {
        ui_print_info(bat_label, bat_buf);
    }
}

static void hw_get_cpu_info(char *out_buf, const size_t buf_size) {
    char model[LINE_BUFFER] = "Unknown";
    uint32_t phy_cores = 0;

    hw_get_cpu_model(model, LINE_BUFFER);
    hw_get_cpu_cores(&phy_cores);
    double freq_ghz = hw_get_cpu_freq(model);
 
    snprintf(out_buf, buf_size, "%s (%u) @ %.02f GHz", model, phy_cores, freq_ghz);
}

static void hw_get_cpu_model(char *model_buf, size_t buf_size) {
    if (sysctlbyname("machdep.cpu.brand_string", model_buf, &buf_size, NULL, 0) != 0) {
        V_PRINTF("[Error] sysctlbyname(machdep.cpu.brand_string) failed: %s\n", strerror(errno));
        return;
    }

    size_t len = strlen(model_buf);
    while (len > 0 && isspace((unsigned char)model_buf[len - 1])) {
        model_buf[--len] = '\0';
    }
}

static void hw_get_cpu_cores(uint32_t *core_count) {
    size_t size = sizeof(*core_count);
    if (sysctlbyname("hw.physicalcpu", core_count, &size, NULL, 0) != 0) {
        V_PRINTF("[Error] sysctlbyname(hw.physicalcpu) failed: %s\n", strerror(errno));
        *core_count = 0;
    }
}

#if TARGET_CPU_ARM64
static double hw_get_cpu_freq(const char *model) {
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
        {3.23, "M1 Ultra"},
        {3.23, "M1 Max"},
        {3.23, "M1 Pro"},
        {3.2, "M1"},
        {3.70, "M2 Ultra"},
        {3.69, "M2 Max"},
        {3.5, "M2 Pro"},
        {3.5, "M2"},
        {4.05, "M3 Ultra"},
        {4.05, "M3 Max"},
        {4.05, "M3 Pro"},
        {4.05, "M3"},
        {4.51, "M4 Max"},
        {4.51, "M4 Pro"},
        {4.40, "M4"},
        {4.61, "M5"},
        {0, NULL}
    };

    for (uint8_t i = 0; m_series[i].model != NULL; i++) {
        if (strstr(model, m_series[i].model) != NULL) {
            return m_series[i].p_frequency;
        }
    }

    return 0.0;
}
#else
static double hw_get_cpu_freq(const char *model) {
    (void)model;

    int64_t hz = 0;
    size_t size = sizeof(hz);

    /* Using nominal maximum frequency (advertised clock speed) */
    if (sysctlbyname("hw.cpufrequency_max", &hz, &size, NULL, 0) == -1) {
        V_PRINTF("[Error] sysctlbyname(hw.cpufrequency_max) failed: %s\n", strerror(errno));
        return 0.0;
    }

    return (double)hz / HZ_PER_GHZ;
}
#endif

#if !TARGET_CPU_ARM64
static void hw_get_gpu_info(char *out_buf, const size_t buf_size) {
    CFMutableDictionaryRef match = IOServiceMatching("IOPCIDevice");
    io_iterator_t it;

    if (IOServiceGetMatchingServices(kIOMainPortDefault, match, &it) != kIOReturnSuccess) {
        V_PRINTF("[Error] IOServiceGetMatchingServices failed to find IOPCIDevice\n");
        return;
    }

    if (strcmp(out_buf, "Unknown") == 0) {
        out_buf[0] = '\0';
    }

    /**
     * PCI class-code struct (Little Endian):
     * code[0] - Prog IF
     * code[1] - Sub-class
     * code[2] - Base Class (0x03 = Display Controller)
     * code[3] - 0x00
     */
    io_registry_entry_t entry;
    while ((entry = IOIteratorNext(it))) {
        CFMutableDictionaryRef props;
        if (IORegistryEntryCreateCFProperties(entry, &props, kCFAllocatorDefault, kNilOptions) == kIOReturnSuccess) {
            CFDataRef class_data = (CFDataRef)CFDictionaryGetValue(props, CFSTR("class-code"));
            if (class_data && CFGetTypeID(class_data) == CFDataGetTypeID() && CFDataGetLength(class_data) >= 4) {
                const uint8_t *code = CFDataGetBytePtr(class_data);
                if (code[2] != 0x03) {
                    CFRelease(props);
                    IOObjectRelease(entry);
                    continue;
                }
            } else {
                CFRelease(props);
                IOObjectRelease(entry);
                continue;
            }

            CFDataRef model = (CFDataRef)CFDictionaryGetValue(props, CFSTR("model"));
            if (model && CFGetTypeID(model) == CFDataGetTypeID()) {
                const char *model_ptr = (const char *)CFDataGetBytePtr(model);
                size_t model_len = CFDataGetLength(model);
                if (model_len > 0 && model_ptr[model_len - 1] == '\0') model_len--;

                size_t current_len = strlen(out_buf);
                if (current_len > 0) {
                    strlcat(out_buf, "\n    ", buf_size);
                    current_len = strlen(out_buf);
                }

                if (current_len < buf_size) {
                    snprintf(out_buf + current_len, buf_size - current_len, "%.*s", (int)model_len, model_ptr);
                }
            }

            CFRelease(props);
        }

        IOObjectRelease(entry);
    }

    if (strlen(out_buf) == 0) {
        V_PRINTF("[Info] No PCI GPU found (might be VM)\n");
        snprintf(out_buf, buf_size, "Unknown");
    }

    IOObjectRelease(it);
}
#endif

static void hw_get_ram_info(char *out_buf, const size_t buf_size) {
    uint64_t total = 0;
    size_t len = sizeof(total);

    if (sysctlbyname("hw.memsize", &total, &len, NULL, 0) != 0) {
        V_PRINTF("[Error] sysctlbyname(hw.memsize) failed: %s\n", strerror(errno));
        return;
    }

    mach_port_t host = mach_host_self();
    vm_statistics64_data_t vm;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_size_t pg;
    if (host_page_size(host, &pg) != KERN_SUCCESS) {
        V_PRINTF("[Error] Mach host_page_size() failed\n");
        return;
    }

    if (host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vm, &count) != KERN_SUCCESS) {
        V_PRINTF("[Error] Mach host_statistics64() failed\n");
        return;
    }

    int64_t wired = (int64_t)vm.wire_count * pg;

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_9
    int64_t active = (int64_t)vm.active_count * pg;
    int64_t inactive = (int64_t)vm.inactive_count * pg;
    int64_t used = active + inactive + wired;
#else
    int64_t app = (int64_t)(vm.internal_page_count - vm.purgeable_count) * pg;
    int64_t compressed = (int64_t)vm.compressor_page_count * pg;
    int64_t used = app + wired + compressed;
#endif

    format_bytes((double)used, (double)total, out_buf, buf_size);
}

static void hw_get_swap_info(char *out_buf, const size_t buf_size) {
    struct xsw_usage xsw;
    size_t xsw_len = sizeof(struct xsw_usage);

    if (sysctlbyname("vm.swapusage", &xsw, &xsw_len, NULL, 0) != 0) {
        V_PRINTF("[Error] sysctlbyname(vm.swapusage) failed: %s\n", strerror(errno));
        return;
    }

    if (xsw.xsu_total == 0) return;

    format_bytes(xsw.xsu_used, xsw.xsu_total, out_buf, buf_size);
}

static void hw_scan_and_print_disks(void) {
    int n = getfsstat(NULL, 0, MNT_WAIT);
    if (n <= 0) {
        V_PRINTF("[Error] getfsstat() size query failed: %s\n", strerror(errno));
        return;
    }

    struct statfs *st = malloc(sizeof(struct statfs) * n);
    if (!st) {
        V_PRINTF("[Error] malloc(%zu) for statfs failed: %s\n", sizeof(struct statfs) * n, strerror(errno));
        return;
    }

    n = getfsstat(st, (int)(sizeof(struct statfs) * n), MNT_NOWAIT);
    if (n <= 0) {
        V_PRINTF("[Error] getfsstat() data fetch failed: %s\n", strerror(errno));
        free(st);
        return;
    }

    string_set_t *outputted_disks = strset_create(INITIAL_CAPACITY);

    for (int i = 0; i < n; i++) {
        if ((strcmp(st[i].f_mntonname, "/") != 0 &&
            strncmp(st[i].f_mntonname, "/Volumes/", 9) != 0)) continue;

        if (strset_contains(outputted_disks, st[i].f_mntfromname)) continue;

        strset_add(outputted_disks, st[i].f_mntfromname);

        uint64_t total = (uint64_t)st[i].f_blocks * st[i].f_bsize;
        uint64_t used = total - ((uint64_t)st[i].f_bfree * st[i].f_bsize);

        char label[LINE_BUFFER], info[LINE_BUFFER];
        snprintf(label, LINE_BUFFER, "Disk (%s)", st[i].f_mntonname);
        format_bytes((double)used, (double)total, info, LINE_BUFFER);

        size_t cur_len = strlen(info);
        snprintf(info + cur_len, LINE_BUFFER - cur_len, " [%s]", st[i].f_fstypename);
        ui_print_info(label, info);
    }

    strset_destroy(outputted_disks);
    free(st);
}

static void hw_get_bat_info(char *label_buf, char *info_buf, const size_t buf_size) {
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    if (!info) {
        V_PRINTF("[Error] IOPSCopyPowerSourcesInfo() failed (no power sources found)\n");
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
        V_PRINTF("[Warning] Battery specific name not found; using fallback: %s\n", kIOPSNameKey);
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

    snprintf(label_buf, buf_size, "Battery (%s)", model);
    snprintf(info_buf, buf_size, "%u%% (%s, Health: %s)", pct, status, health);

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

static const char* hw_get_bat_status(const CFDictionaryRef power_source, uint8_t battery_percentage) {
    uint8_t status = 0;
    CFStringRef state = CFDictionaryGetValue(power_source, CFSTR(kIOPSPowerSourceStateKey));
    CFBooleanRef charging = CFDictionaryGetValue(power_source, CFSTR(kIOPSIsChargingKey));

    if (state && CFStringCompare(state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo) {
        status |= FLAG_AC;
    }
    if (charging && CFBooleanGetValue(charging)) status |= FLAG_CHARGING;
    if (battery_percentage >= 100) status |= FLAG_FULL;

    if (status & FLAG_FULL && status & FLAG_AC) return "Full";
    if (status & FLAG_CHARGING) return "Charging";
    if (status & FLAG_AC) return "Not Charging";
    return "Discharging";
}

static void format_bytes(double used_size, double total_size, char *out_buf, 
                         const size_t buf_size) {
    static const char *memory_units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};

    static const double divisors[] = {
        1.0,               /* B */
        1024.0,            /* KiB */
        1048576.0,         /* MiB */
        1073741824.0,      /* GiB */
        1099511627776.0,   /* TiB */
        1125899906842624.0 /* PiB */
    };

    const uint8_t usage_pct = (total_size > 1e-9) ? (uint8_t)((used_size / total_size) * 100) : 0;

    int8_t forced_unit = -1;
    uint8_t precision = 2;

    if (cfg_is_kib()) { forced_unit = 1; precision = 0; }
    if (cfg_is_mib()) { forced_unit = 2; precision = 0; }
    if (cfg_is_gib()) { forced_unit = 3; precision = 2; }

    uint8_t total_unit = 0;
    uint8_t used_unit = 0;

    if (forced_unit != -1) {
        total_unit = forced_unit;
        used_unit = forced_unit;

        total_size /= divisors[forced_unit];
        used_size /= divisors[forced_unit];
    } else {
        uint8_t array_border = (sizeof(memory_units) / sizeof(memory_units[0]));

        while (total_size >= 1024 && total_unit < array_border - 1) {
            total_size /= 1024;
            total_unit++;
        }

        while (used_size >= 1024 && used_unit < array_border - 1) {
            used_size /= 1024;
            used_unit++;
        }
    }

    snprintf(out_buf, buf_size, "%.*f %s / %.*f %s (%hhu%%)", 
             precision, used_size, memory_units[used_unit], 
             precision, total_size, memory_units[total_unit], usage_pct);
}
