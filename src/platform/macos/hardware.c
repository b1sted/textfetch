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

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>

#include "hardware.h"
#include "ui.h"

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
 * Aggregates CPU model, physical core count, and frequency into a string.
 * 
 * @param out_buf  The destination buffer.
 * @param buf_size The size of the destination buffer.
 */
static void hw_get_cpu_info(char *out_buf, const size_t buf_size);

/**
 * Retrieves the CPU brand string using sysctl (machdep.cpu.brand_string).
 * Trims trailing whitespace often present in the sysctl output.
 * 
 * @param model_buf The buffer to store the model name.
 * @param buf_size  The size of the buffer.
 */
static void hw_get_cpu_model(char *model_buf, size_t buf_size);

/**
 * Gets the number of physical (non-logical) CPU cores.
 * 
 * @param core_count Pointer to store the count.
 */
static void hw_get_cpu_cores(uint32_t *core_count);

/**
 * Retrieves the maximum CPU frequency in GHz using sysctl.
 * 
 * @param cpu_freq Pointer to store the result as a double.
 */
static void hw_get_cpu_freq(double *cpu_freq);

/**
 * GPU Detection Helpers
 * Uses IOKit IORegistry to scan for IOPCIDevice nodes and extract 
 * the "model" property.
 */

/**
 * Iterates through the I/O Registry to find PCI display devices.
 * Extracts the model name from the device properties.
 * 
 * @param out_buf  The destination buffer.
 * @param buf_size The size of the destination buffer.
 */
static void hw_get_gpu_info(char *out_buf, const size_t buf_size);

/**
 * Memory and Storage Helpers
 * Uses Mach VM statistics for RAM and statfs for disk usage.
 */

/**
 * Calculates RAM usage following Activity Monitor logic.
 * Formula: Used = (Internal - Purgeable) + Wired + Compressed.
 * 
 * @param out_buf  Buffer for formatted RAM info.
 * @param buf_size Size of the buffer.
 */
static void hw_get_ram_info(char *out_buf, const size_t buf_size);

/**
 * Retrieves Swap usage via the vm.swapusage sysctl node.
 * 
 * @param out_buf  Buffer for formatted Swap info.
 * @param buf_size Size of the buffer.
 */
static void hw_get_swap_info(char *out_buf, const size_t buf_size);

/**
 * Iterates through all mounted file systems using getfsstat.
 * Filters results to show only the root and user-mounted volumes.
 */
static void hw_scan_and_print_disks(void);

/**
 * Power Supply Helpers
 * Uses IOPowerSources API to detect batteries and their status.
 */

/**
 * Collects battery hardware model, health, and charging status.
 * 
 * @param label_buf Buffer for the "Battery (Model)" label.
 * @param info_buf  Buffer for the "Percentage% (Status)" string.
 * @param buf_size  Size of the buffers.
 */
static void hw_get_bat_info(char *label_buf, char *info_buf, const size_t buf_size);

/**
 * Calculates battery percentage based on current and max capacity.
 * 
 * @param power_source A dictionary containing power source properties.
 * @return The calculated percentage (0-100).
 */
static uint8_t hw_get_bat_percentage(const CFDictionaryRef power_source);

/**
 * Determines the charging status string (Charging, Discharging, etc.)
 * 
 * @param power_source       A dictionary containing power source properties.
 * @param battery_percentage Current percentage to check for "Full" state.
 * @return A constant string representing the battery status.
 */
static const char* hw_get_bat_status(const CFDictionaryRef power_source, uint8_t battery_percentage);

/**
 * Utility: Human-readable byte formatting.
 * 
 * Scales byte values to KiB, MiB, GiB, etc., based on configuration.
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
    hw_get_gpu_info(gpu_buf, LINE_BUFFER);
    ui_print_info("GPU", gpu_buf);

    char ram_buf[LINE_BUFFER] = "";
    hw_get_ram_info(ram_buf, LINE_BUFFER);
    if (strlen(ram_buf) > 0) {
        ui_print_info("RAM", ram_buf);
    }

    char swap_buf[LINE_BUFFER] = "";
    hw_get_swap_info(ram_buf, LINE_BUFFER);
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
    uint32_t phy_cores = 0;
    double freq_ghz = 0;
    char model[LINE_BUFFER] = "Unknown CPU";

    hw_get_cpu_model(model, LINE_BUFFER);
    hw_get_cpu_cores(&phy_cores);
    hw_get_cpu_freq(&freq_ghz);
 
    snprintf(out_buf, buf_size, "%s (%u) @ %.03f GHz", 
             model, phy_cores, freq_ghz);
}

static void hw_get_cpu_model(char *model_buf, size_t buf_size) {
    if (sysctlbyname("machdep.cpu.brand_string", model_buf, &buf_size, NULL, 0) != 0) {
        V_PRINTF("Error: failed to retrieve CPU brand string\n");
        return;
    }

    size_t len = strlen(model_buf);
    while (len > 0 && isspace((unsigned char)model_buf[len - 1])) {
        model_buf[--len] = '\0';
    }
}

static void hw_get_cpu_cores(uint32_t *core_count) {
    size_t size = sizeof(core_count);
    if (sysctlbyname("hw.physicalcpu", core_count, &size, NULL, 0) != 0) {
        V_PRINTF("Error: failed to get physical CPU cores\n");
    }
}

static void hw_get_cpu_freq(double *cpu_freq) {
    int64_t hz = 0;
    size_t size = sizeof(hz);

    /* Using max freq as it's most stable */
    if (sysctlbyname("hw.cpufrequency_max", &hz, &size, NULL, 0) == -1) {
        V_PRINTF("Error: failed to get CPU frequency\n");
        return;
    }

    *cpu_freq = (double)hz / 1e9;
}

static void hw_get_gpu_info(char *out_buf, const size_t buf_size) {
    CFMutableDictionaryRef match = IOServiceMatching("IOPCIDevice");
    io_iterator_t it;

    if (IOServiceGetMatchingServices(kIOMainPortDefault, match, &it) != kIOReturnSuccess) {
        return;
    }

    io_registry_entry_t entry;
    while ((entry = IOIteratorNext(it))) {
        CFMutableDictionaryRef props;
        if (IORegistryEntryCreateCFProperties(entry, &props, kCFAllocatorDefault, kNilOptions) == kIOReturnSuccess) {
            CFDataRef model = (CFDataRef)CFDictionaryGetValue(props, CFSTR("model"));
            if (model) {
                snprintf(out_buf, buf_size, "%s", (const char *)CFDataGetBytePtr(model));
            }
            CFRelease(props);
        }
        IOObjectRelease(entry);
    }
    IOObjectRelease(it);
}

static void hw_get_ram_info(char *out_buf, const size_t buf_size) {
    uint64_t total = 0;
    size_t len = sizeof(total);

    if (sysctlbyname("hw.memsize", &total, &len, NULL, 0) != 0) {
        V_PRINTF("Error: failed to get memory size\n");
        return;
    }

    mach_port_t host = mach_host_self();
    vm_statistics64_data_t vm;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_size_t pg;

    if (host_page_size(host, &pg) != KERN_SUCCESS ||
        host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vm, &count) != KERN_SUCCESS) {
        V_PRINTF("Error: failed to retrieve Mach VM statistics\n");
        return;
    }

    int64_t app = (int64_t)(vm.internal_page_count - vm.purgeable_count) * pg;
    int64_t wired = (int64_t)vm.wire_count * pg;
    int64_t compressed = (int64_t)vm.compressor_page_count * pg;
    int64_t used = app + wired + compressed;
    
    format_bytes((double)used, (double)total, out_buf, buf_size);
}

static void hw_get_swap_info(char *out_buf, const size_t buf_size) {
    struct xsw_usage xsw;
    size_t swap_len = sizeof(struct xsw_usage);
    if (sysctlbyname("vm.swapusage", &xsw, &swap_len, NULL, 0) != 0 || 
        xsw.xsu_total == 0) return;

    format_bytes(xsw.xsu_used, xsw.xsu_total, out_buf, buf_size);
}

static void hw_scan_and_print_disks(void) {
    int n = getfsstat(NULL, 0, MNT_WAIT);
    if (n <= 0) return;

    struct statfs *st = malloc(sizeof(struct statfs) * n);
    n = getfsstat(st, (int)(sizeof(struct statfs) * n), MNT_NOWAIT);

    for (int i = 0; i < n; i++) {
        if ((strcmp(st[i].f_mntonname, "/") != 0 &&
            strncmp(st[i].f_mntonname, "/Volumes/", 9) != 0)) continue;

        uint64_t total = (uint64_t)st[i].f_blocks * st[i].f_bsize;
        uint64_t used = total - ((uint64_t)st[i].f_bfree * st[i].f_bsize);

        char label[LINE_BUFFER], info[LINE_BUFFER];
        snprintf(label, LINE_BUFFER, "Disk (%s)", st[i].f_mntonname);
        format_bytes((double)used, (double)total, info, LINE_BUFFER);

        size_t cur_len = strlen(info);
        snprintf(info + cur_len, LINE_BUFFER - cur_len, " [%s]", st[i].f_fstypename);
        ui_print_info(label, info);
    }

    free(st);
}

static void hw_get_bat_info(char *label_buf, char *info_buf, const size_t buf_size) {
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    CFArrayRef list = IOPSCopyPowerSourcesList(info);

    if (!list || CFArrayGetCount(list) == 0) {
        if (list) CFRelease(list);
        CFRelease(info);
        return;
    }

    CFDictionaryRef dict = IOPSGetPowerSourceDescription(info, 
                                          CFArrayGetValueAtIndex(list, 0));

    char model[SMALL_BUFFER] = "Unknown";
    CFStringRef model_ref = CFDictionaryGetValue(dict, CFSTR("Model"));
    if (model_ref) {
         CFStringGetCString(model_ref, model, 
                            sizeof(model), kCFStringEncodingUTF8);
    }

    char health[SMALL_BUFFER] = "Unknown";
    CFStringRef health_ref = CFDictionaryGetValue(dict, CFSTR(kIOPSBatteryHealthKey));
    if (health_ref) {
         CFStringGetCString(health_ref, health, 
                            sizeof(health), kCFStringEncodingUTF8);
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

    return (max_ref > 0) ? (uint8_t)((double)cur_cap / max_cap * 100) : 0;
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
    const char *memory_units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    uint8_t usage_pct = (total_size > 0) ? (uint8_t)((used_size / total_size) * 100) : 0;

    if (cfg_is_kib()) {
        total_size /= BYTES_TO_KIB_DIVISOR;
        used_size /= BYTES_TO_KIB_DIVISOR;

        snprintf(out_buf, buf_size, "%.0f %s / %.0f %s (%hhu%%)", 
                 used_size, memory_units[1], 
                 total_size, memory_units[1], usage_pct);

        return;
    }

    if (cfg_is_mib()) {
        total_size /= BYTES_TO_MIB_DIVISOR;
        used_size /= BYTES_TO_MIB_DIVISOR;
        
        snprintf(out_buf, buf_size, "%.0f %s / %.0f %s (%hhu%%)", 
                 used_size, memory_units[2], 
                 total_size, memory_units[2], usage_pct);
        
        return;
    }

    if (cfg_is_gib()) {
        total_size /= BYTES_TO_GIB_DIVISOR;
        used_size /= BYTES_TO_GIB_DIVISOR;
        
        snprintf(out_buf, buf_size, "%.2f %s / %.2f %s (%hhu%%)", 
                 used_size, memory_units[3], 
                 total_size, memory_units[3], usage_pct);
    
        return;
    }

    int i = 0;
    while (total_size >= 1024 && i < 5) {
        total_size /= 1024;
        i++;
    }

    int j = 0;
    while (used_size >= 1024 && j < 5) {
        used_size /= 1024;
        j++;
    }

    snprintf(out_buf, buf_size, "%.2f %s / %.2f %s (%hhu%%)", 
             used_size, memory_units[j], 
             total_size, memory_units[i], usage_pct);
}
