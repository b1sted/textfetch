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
#include <mach/mach_port.h>
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

/* IOKit service class exposing the Apple Silicon power-management node. */
#define PMGR_SERVICE_CLASS   "AppleARMIODevice"

/* PMGR node names used by current and newer Apple Silicon generations. */
#define PMGR_NODE_NAME       "pmgr"
#define PMGR_CHILD_NODE_NAME "pmgr-child"

/* Prefix shared by the binary DVFS tables published by PMGR. */
#define DVFS_PROPERTY_PREFIX "voltage-states"

/* Each DVFS state is a little-endian (frequency, voltage) pair of uint32_t. */
#define DVFS_ENTRY_BYTES     8U

/* Accepted frequency ranges for tables expressed in Hz and kHz. */
#define CPU_FREQ_MIN_HZ      100000000ULL
#define CPU_FREQ_MAX_HZ      10000000000ULL
#define CPU_FREQ_MIN_KHZ     100000ULL
#define CPU_FREQ_MAX_KHZ     10000000ULL
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
 * @param cpu Pointer to the CPU info structure.
 */
static void hw_get_cpu_package(cpu_info_t *cpu);

/**
 * Retrieves the CPU model name string via sysctl.
 *
 * @param cpu Pointer to the CPU info structure.
 */
static void hw_get_cpu_model(cpu_info_t *cpu);

/**
 * Retrieves the physical core counts, separated by P and E cores on ARM64.
 *
 * @param cpu Pointer to the CPU info structure.
 */
static void hw_get_cpu_cores(cpu_info_t *cpu);

/**
 * Retrieves the maximum CPU frequency (IOKit DVFS probe with a static
 * fallback for ARM64, sysctl for x86).
 *
 * @param cpu Pointer to the CPU info structure.
 */
static void hw_get_cpu_freq(cpu_info_t *cpu);

#if TARGET_CPU_ARM64
/**
 * Probes the IOKit PMGR DVFS tables for the highest CPU clock speed.
 *
 * The lookup is best-effort: VMs and unsupported SoCs have no usable PMGR
 * node, in which case zero is returned so callers can use a static fallback.
 *
 * @return Peak frequency in Hz, or 0 when no valid DVFS table was found.
 */
static uint64_t hw_get_arm_cpu_freq(void);
#endif

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
 * @param mem Pointer to the memory info block.
 */
static void hw_get_ram_info(mem_flags_t *flags, mem_info_t *mem);

/**
 * Evaluates swap usage using sysctl vm.swapusage.
 *
 * @param flags Bitmask storing the detected memory types.
 * @param mem Pointer to the memory info block.
 */
static void hw_get_swap_info(mem_flags_t *flags, mem_info_t *mem);

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
    cpu_info_t cpu;
    memset(&cpu, 0, sizeof(cpu_info_t));

    hw_get_cpu_package(&cpu);
    hw_get_cpu_model(&cpu);
    hw_get_cpu_cores(&cpu);
    hw_get_cpu_freq(&cpu);

    char cpu_info[LINE_BUFFER] = {0};
    snprintf(cpu_info, sizeof(cpu_info), "%s%s%s", cpu.model, cpu.cores, cpu.frequency);

    ui_print_info("CPU", cpu_info);
}

static void hw_get_cpu_package(cpu_info_t *cpu) {
    uint32_t packages = 0;
    size_t packages_size = sizeof(packages);

    if (sysctlbyname("hw.packages", &packages, &packages_size, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(hw.packages) failed: %s\n", strerror(errno));
    }

    cpu->packages = (packages) ? (uint8_t)packages : 1;
}

static void hw_get_cpu_model(cpu_info_t *cpu) {
    size_t model_size = sizeof(cpu->model);

    if (sysctlbyname("machdep.cpu.brand_string", cpu->model, &model_size, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(machdep.cpu.brand_string) failed: %s\n", strerror(errno));
    }

    if (cpu->model[0] == '\0') {
        snprintf(cpu->model, sizeof(cpu->model), "Unknown");
        return;
    }

    if (cpu->packages > 1) {
        char prefix[TINY_BUFFER] = {0};
        snprintf(prefix, sizeof(prefix), "%" PRIu32 " x ", cpu->packages);
        size_t prefix_len = strlen(prefix);

        size_t model_len = strlen(cpu->model);
        if (model_len + prefix_len < sizeof(cpu->model)) {
            memmove(cpu->model + prefix_len, cpu->model, model_len + 1);
            memcpy(cpu->model, prefix, prefix_len);
        }
    }

    size_t len = strlen(cpu->model);
    while (len > 0 && isspace((unsigned char)cpu->model[len - 1])) {
        cpu->model[--len] = '\0';
    }
}

static void hw_get_cpu_cores(cpu_info_t *cpu) {
    uint64_t common_cores = 0;
    size_t common_size = sizeof(common_cores);

    if (sysctlbyname("hw.physicalcpu", &common_cores, &common_size, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(hw.physicalcpu) failed: %s\n", strerror(errno));
    }

    size_t core_size = sizeof(cpu->cores);
    snprintf(cpu->cores, core_size, " (%" PRIu64 ")", common_cores);

#if TARGET_CPU_ARM64
    uint64_t p_cores = 0;
    size_t p_size = sizeof(p_cores);

    if (sysctlbyname("hw.perflevel0.physicalcpu", &p_cores, &p_size, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(hw.perflevel0.physicalcpu) failed: %s\n", strerror(errno));
    }

    uint64_t e_cores = 0;
    size_t e_size = sizeof(e_cores);

    if (sysctlbyname("hw.perflevel1.physicalcpu", &e_cores, &e_size, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(hw.perflevel1.physicalcpu) failed: %s\n", strerror(errno));
    }

    size_t current_len = strlen(cpu->cores) - 1;
    snprintf(cpu->cores + current_len, core_size - current_len,
             "-core: %" PRIu64 "P + %" PRIu64 "E)", p_cores, e_cores);
#endif
}

#if TARGET_CPU_ARM64
/**
 * Reads an unaligned little-endian 32-bit value from a byte buffer.
 *
 * PMGR property data is not guaranteed to be naturally aligned, so byte-wise
 * assembly avoids unaligned access penalties and sanitizer complaints.
 *
 * @param bytes Pointer to the first byte of the value.
 * @return The decoded 32-bit value.
 */
static uint32_t dvfs_read_le32(const uint8_t *bytes) {
    return (uint32_t)bytes[0]
         | ((uint32_t)bytes[1] << 8)
         | ((uint32_t)bytes[2] << 16)
         | ((uint32_t)bytes[3] << 24);
}

/**
 * Normalizes a raw DVFS frequency field to Hz.
 *
 * M1-M3 PMGR tables store frequencies in Hz, while M4 and newer tables use
 * kHz. Values outside both plausible CPU ranges are rejected as malformed.
 *
 * @param raw Frequency field as stored in the DVFS table.
 * @return Frequency in Hz, or 0 when the value is not a plausible CPU clock.
 */
static uint64_t dvfs_normalize_frequency(const uint32_t raw) {
    uint64_t frequency = raw;

    if (frequency >= CPU_FREQ_MIN_HZ && frequency <= CPU_FREQ_MAX_HZ) {
        return frequency;
    }

    if (frequency >= CPU_FREQ_MIN_KHZ && frequency <= CPU_FREQ_MAX_KHZ) {
        return frequency * 1000ULL;
    }

    return 0;
}

/**
 * Finds the highest frequency in one binary DVFS state table.
 *
 * @param data CoreFoundation data containing (frequency, voltage) pairs.
 * @return Peak frequency in Hz, or 0 when the table is empty or malformed.
 */
static uint64_t dvfs_peak_frequency(CFDataRef data) {
    if (!data || CFGetTypeID(data) != CFDataGetTypeID()) return 0;

    CFIndex length = CFDataGetLength(data);
    const uint8_t *bytes = CFDataGetBytePtr(data);

    if (!bytes || length < (CFIndex)DVFS_ENTRY_BYTES) return 0;

    uint64_t peak = 0;

    for (CFIndex offset = 0;
         offset + (CFIndex)DVFS_ENTRY_BYTES <= length;
         offset += (CFIndex)DVFS_ENTRY_BYTES) {
        uint64_t frequency = dvfs_normalize_frequency(dvfs_read_le32(bytes + offset));
        if (frequency > peak) peak = frequency;
    }

    return peak;
}

/**
 * Checks whether a PMGR property name belongs to a DVFS frequency table.
 *
 * @param key Property dictionary key.
 * @return true when the key is a CFString starting with "voltage-states".
 */
static bool dvfs_is_property_name(CFTypeRef key) {
    if (!key || CFGetTypeID(key) != CFStringGetTypeID()) return false;

    char name[SMALL_BUFFER] = {0};
    if (!CFStringGetCString((CFStringRef)key, name, sizeof(name),
                            kCFStringEncodingUTF8)) {
        return false;
    }

    return strncmp(name, DVFS_PROPERTY_PREFIX,
                   sizeof(DVFS_PROPERTY_PREFIX) - 1) == 0;
}

/**
 * Scans all DVFS properties in a PMGR property dictionary.
 *
 * @param properties Property dictionary copied from an AppleARMIODevice node.
 * @return Highest frequency across all voltage-states tables, in Hz.
 */
static uint64_t dvfs_peak_from_properties(CFDictionaryRef properties) {
    if (!properties || CFGetTypeID(properties) != CFDictionaryGetTypeID()) {
        return 0;
    }

    CFIndex count = CFDictionaryGetCount(properties);
    if (count <= 0) return 0;

    const void **keys = malloc((size_t)count * sizeof(*keys));
    const void **values = malloc((size_t)count * sizeof(*values));

    if (!keys || !values) {
        free(keys);
        free(values);
        return 0;
    }

    CFDictionaryGetKeysAndValues(properties, keys, values);

    uint64_t peak = 0;

    for (CFIndex i = 0; i < count; i++) {
        if (!dvfs_is_property_name(keys[i])) continue;

        CFTypeRef value = values[i];
        if (!value || CFGetTypeID(value) != CFDataGetTypeID()) continue;

        uint64_t frequency = dvfs_peak_frequency((CFDataRef)value);
        if (frequency > peak) peak = frequency;
    }

    free(keys);
    free(values);

    return peak;
}

/**
 * Checks whether an IORegistry entry name refers to a PMGR node.
 *
 * Newer Apple Silicon generations move the DVFS tables to "pmgr-child" while
 * leaving "pmgr" as a stub, so both names are accepted.
 *
 * @param name IORegistry entry name.
 * @return true for a PMGR node.
 */
static bool is_pmgr_node_name(const char *name) {
    return name != NULL &&
           (strcmp(name, PMGR_NODE_NAME) == 0 ||
            strcmp(name, PMGR_CHILD_NODE_NAME) == 0);
}

static uint64_t hw_get_arm_cpu_freq(void) {
    io_iterator_t iterator = get_matching_iterator(PMGR_SERVICE_CLASS);
    if (iterator == IO_OBJECT_NULL) return 0;

    uint64_t peak = 0;
    io_service_t service;

    while ((service = IOIteratorNext(iterator))) {
        char name[MEDIUM_BUFFER] = {0};

        if (IORegistryEntryGetName(service, name) == kIOReturnSuccess &&
            is_pmgr_node_name(name)) {
            CFMutableDictionaryRef properties = NULL;

            if (IORegistryEntryCreateCFProperties(service, &properties,
                                                  kCFAllocatorDefault, 0) == kIOReturnSuccess &&
                properties) {
                uint64_t candidate = dvfs_peak_from_properties(properties);
                if (candidate > peak) peak = candidate;

                CFRelease(properties);
            }
        }

        IOObjectRelease(service);

        if (peak > 0) break;
    }

    IOObjectRelease(iterator);

    return peak;
}

static void hw_get_cpu_freq(cpu_info_t *cpu) {
    if (cpu == NULL) return;

    /*
     * Prefer the live PMGR DVFS tables when the running SoC exposes them.
     * This keeps the reported peak P-core clock accurate on new hardware
     * without requiring a table update for every Apple Silicon revision.
     */
    uint64_t peak_hz = hw_get_arm_cpu_freq();
    if (peak_hz > 0) {
        snprintf(cpu->frequency, sizeof(cpu->frequency),
                 " @ %.2f GHz", (double)peak_hz / HZ_PER_GHZ);
        return;
    }

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
        {4.61, 3.05, "M5 Max"},
        {4.61, 3.05, "M5 Pro"},
        {4.61, 3.05, "M5"},
        {0, 0, NULL}
    };

    for (size_t i = 0; m_series[i].model != NULL; i++) {
        if (strstr(cpu->model, m_series[i].model) != NULL) {
            snprintf(cpu->frequency, sizeof(cpu->frequency),
                     " @ %.2f / %.2f GHz", m_series[i].p_frequency,
                     m_series[i].e_frequency);
            return;
        }
    }
}
#else
static void hw_get_cpu_freq(cpu_info_t *cpu) {
    uint64_t hz = 0;
    size_t size = sizeof(hz);

    /* Using nominal maximum frequency (advertised clock speed) */
    if (sysctlbyname("hw.cpufrequency_max", &hz, &size, NULL, 0) == -1) {
        V_PRINTF("[ERROR] sysctlbyname(hw.cpufrequency_max) failed: %s\n", strerror(errno));
    }

    snprintf(cpu->frequency, sizeof(cpu->frequency), " @ %.2f GHz", (double)hz / HZ_PER_GHZ);
}
#endif

/**
 * Copies a string-like CoreFoundation value into a NUL-terminated buffer.
 *
 * IOKit exposes some string properties as CFData, which carries no guaranteed
 * terminator. CFString values are converted to UTF-8 when possible.
 *
 * @param value The CoreFoundation object to read.
 * @param out_buf Destination buffer.
 * @param buf_size Size of the destination buffer.
 * @return true if a non-empty string was copied.
 */
static bool cf_value_to_string(CFTypeRef value, char *out_buf, const size_t buf_size) {
    if (!value || !out_buf || buf_size == 0) return false;

    out_buf[0] = '\0';

    CFTypeID type = CFGetTypeID(value);

    if (type == CFStringGetTypeID()) {
        return CFStringGetCString((CFStringRef)value, out_buf,
                                  (CFIndex)buf_size, kCFStringEncodingUTF8) != 0;
    }

    if (type != CFDataGetTypeID()) return false;

    CFDataRef data = (CFDataRef)value;
    CFIndex length = CFDataGetLength(data);
    const uint8_t *bytes = CFDataGetBytePtr(data);

    if (!bytes || length <= 0) return false;

    size_t copy = (size_t)length;
    if (copy >= buf_size) copy = buf_size - 1;

    size_t actual = 0;
    while (actual < copy && bytes[actual] != '\0') actual++;

    if (actual == 0) return false;

    memcpy(out_buf, bytes, actual);
    out_buf[actual] = '\0';

    return true;
}

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

                if (cf_value_to_string(model, name, sizeof(name))) {
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
                if (CFGetTypeID(class_code) == CFDataGetTypeID() &&
                    CFDataGetLength((CFDataRef)class_code) >= 3) {
                    const uint8_t *code = CFDataGetBytePtr((CFDataRef)class_code);

                    if (code && code[2] == 0x03) {
                        CFTypeRef model = IORegistryEntryCreateCFProperty(entry, CFSTR("model"), kCFAllocatorDefault, 0);
                        if (model) {
                            char name[MEDIUM_BUFFER] = {0};

                            if (cf_value_to_string(model, name, sizeof(name))) {
                                if (out_buf[0] != '\0') strlcat(out_buf, "\n     ", buf_size);
                                strlcat(out_buf, name, buf_size);
                            }

                            CFRelease(model);
                        }
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
    mem_info_t mem;
    memset(&mem, 0, sizeof(mem_info_t));

    mem_flags_t flags = 0;

    hw_get_ram_info(&flags, &mem);
    hw_get_swap_info(&flags, &mem);
    hw_print_mem_info(flags, &mem);
}

static void hw_get_ram_info(mem_flags_t *flags, mem_info_t *mem) {
    size_t len = sizeof(mem->ram_size);

    if (sysctlbyname("hw.memsize", &mem->ram_size, &len, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(hw.memsize) failed: %s\n", strerror(errno));
        return;
    }

    if (mem->ram_size == 0) return;

    mach_port_t host = mach_host_self();
    vm_statistics64_data_t vm = {0};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_size_t pg;

    if (host_page_size(host, &pg) != KERN_SUCCESS) {
        V_PRINTF("[ERROR] Mach host_page_size() failed\n");
        mach_port_deallocate(mach_task_self(), host);
        return;
    }

    if (host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vm, &count) != KERN_SUCCESS) {
        V_PRINTF("[ERROR] Mach host_statistics64() failed\n");
        mach_port_deallocate(mach_task_self(), host);
        return;
    }

    mach_port_deallocate(mach_task_self(), host);

    int64_t wired = (int64_t)vm.wire_count * pg;

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_9
    int64_t active = (int64_t)vm.active_count * pg;
    int64_t inactive = (int64_t)vm.inactive_count * pg;
    mem->ram_used = active + inactive + wired;
#else
    int64_t app = (int64_t)(vm.internal_page_count - vm.purgeable_count) * pg;
    int64_t compressed = (int64_t)vm.compressor_page_count * pg;
    mem->ram_used = app + wired + compressed;
#endif

    /*
     * Kernel memory counters can briefly exceed the installed capacity while
     * pages are being reclaimed or compressed. Clamp the result so callers
     * never report more than 100% RAM usage.
     */
    if (mem->ram_used > mem->ram_size) {
        mem->ram_used = mem->ram_size;
    }

    *flags |= MEM_RAM;
}

static void hw_get_swap_info(mem_flags_t *flags, mem_info_t *mem) {
    struct xsw_usage xsw;
    size_t xsw_len = sizeof(struct xsw_usage);

    if (sysctlbyname("vm.swapusage", &xsw, &xsw_len, NULL, 0) != 0) {
        V_PRINTF("[ERROR] sysctlbyname(vm.swapusage) failed: %s\n", strerror(errno));
        return;
    }

    if (xsw.xsu_total == 0) return;

    *flags |= MEM_SWAP;

    mem->swap_size = xsw.xsu_total;
    mem->swap_used = xsw.xsu_used;
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
    if (!dict || CFGetTypeID(dict) != CFDictionaryGetTypeID()) {
        V_PRINTF("[ERROR] IOPSGetPowerSourceDescription() returned an invalid description\n");
        CFRelease(list);
        CFRelease(info);
        return;
    }

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
    if (health_ref && CFGetTypeID(health_ref) == CFStringGetTypeID()) {
        CFStringGetCString(health_ref, health, sizeof(health), kCFStringEncodingUTF8);
    }

    uint8_t pct = hw_get_bat_percentage(dict);
    const char *status = hw_get_bat_status(dict, pct);

    char label_buf[LINE_BUFFER] = {0};
    char info_buf[MEDIUM_BUFFER] = {0};

    snprintf(label_buf, sizeof(label_buf), "Battery (%s)", model);
    snprintf(info_buf, sizeof(info_buf), "%u%% (%s, Health: %s)", pct, status, health);

    ui_print_info(label_buf, info_buf);

    CFRelease(list);
    CFRelease(info);
}

static uint8_t hw_get_bat_percentage(const CFDictionaryRef power_source) {
    if (!power_source || CFGetTypeID(power_source) != CFDictionaryGetTypeID()) return 0;

    int cur_cap = 0, max_cap = 0;

    CFNumberRef cur_ref = CFDictionaryGetValue(power_source, CFSTR(kIOPSCurrentCapacityKey));
    CFNumberRef max_ref = CFDictionaryGetValue(power_source, CFSTR(kIOPSMaxCapacityKey));

    if (cur_ref && CFGetTypeID(cur_ref) == CFNumberGetTypeID()) {
        CFNumberGetValue(cur_ref, kCFNumberIntType, &cur_cap);
    }
    if (max_ref && CFGetTypeID(max_ref) == CFNumberGetTypeID()) {
        CFNumberGetValue(max_ref, kCFNumberIntType, &max_cap);
    }

    /* Check max_cap > 0 to avoid division by zero if battery is reporting error */
    if (max_cap <= 0) return 0;

    double percentage = ((double)cur_cap / (double)max_cap) * 100.0;
    if (percentage < 0.0) percentage = 0.0;
    if (percentage > 100.0) percentage = 100.0;

    return (uint8_t)percentage;
}

static const char *hw_get_bat_status(const CFDictionaryRef power_source,
                                     uint8_t battery_percentage) {
    if (!power_source || CFGetTypeID(power_source) != CFDictionaryGetTypeID()) {
        return "Unknown";
    }

    CFStringRef state = CFDictionaryGetValue(power_source, CFSTR(kIOPSPowerSourceStateKey));

    CFBooleanRef is_charging = CFDictionaryGetValue(power_source, CFSTR(kIOPSIsChargingKey));
    CFBooleanRef is_charged  = CFDictionaryGetValue(power_source, CFSTR(kIOPSIsChargedKey));

    uint8_t status = 0;

    if (state && CFGetTypeID(state) == CFStringGetTypeID() &&
        CFStringCompare(state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo)
        status |= FLAG_AC;
    if (is_charging && CFGetTypeID(is_charging) == CFBooleanGetTypeID() &&
        CFBooleanGetValue(is_charging))
        status |= FLAG_CHARGING;
    if ((is_charged && CFGetTypeID(is_charged) == CFBooleanGetTypeID() &&
         CFBooleanGetValue(is_charged)) || (battery_percentage >= 100))
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
