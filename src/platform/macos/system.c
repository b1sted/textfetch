/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h> 

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <TargetConditionals.h>

#include <CoreFoundation/CoreFoundation.h>

#include "capture.h"
#include "system.h"
#include "ui.h"

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_VERSION_11_0
#define NANOSECONDS_IN_SECONDS 1e9
#endif

/**
 * Mapping structure to associate Darwin kernel major versions 
 * with macOS marketing/release names.
 */
typedef struct {
    int major;
    const char *name;
} macos_name_map_t;

static struct utsname sys_info;

/**
 * Darwin major version map.
 * Darwin version = macOS version + 9 (for macOS 11+).
 * Darwin version = macOS minor + 4 (for Mac OS X 10.x).
 */
static const macos_name_map_t darwin_map[] = {
    {25, "macOS Tahoe"},           /* macOS 26 */
    {24, "macOS Sequoia"},         /* macOS 15 */
    {23, "macOS Sonoma"},          /* macOS 14 */
    {22, "macOS Ventura"},         /* macOS 13 */
    {21, "macOS Monterey"},        /* macOS 12 */
    {20, "macOS Big Sur"},         /* macOS 11 */
    {19, "macOS Catalina"},        /* 10.15 */
    {18, "macOS Mojave"},          /* 10.14 */
    {17, "macOS High Sierra"},     /* 10.13 */
    {16, "macOS Sierra"},          /* 10.12 */
    {15, "OS X El Capitan"},       /* 10.11 */
    {14, "OS X Yosemite"},         /* 10.10 */
    {13, "OS X Mavericks"},        /* 10.9 */
    {12, "OS X Mountain Lion"},    /* 10.8 */
    {11, "Mac OS X Lion"},         /* 10.7 */
    {10, "Mac OS X Snow Leopard"}, /* 10.6 */
    {9,  "Mac OS X Leopard"},      /* 10.5 */
    {8,  "Mac OS X Tiger"},        /* 10.4 */
    {7,  "Mac OS X Panther"},      /* 10.3 */
    {6,  "Mac OS X Jaguar"},       /* 10.2 */
    {5,  "Mac OS X Puma"},         /* 10.1 */
    {0,  NULL}
};

/**
 * Internal helper to retrieve the effective username.
 * Queries the system password database (getpwuid) using the current 
 * effective user ID. Falls back to "unknown" on failure.
 *
 * @param out_buf  Destination buffer for the username.
 * @param buf_size Maximum size of the destination buffer.
 */
static void sys_get_identity(char *out_buf, const size_t buf_size);

/**
 * Internal helper to identify the OS distribution and version.
 * First attempts to use sysctl (kern.osproductversion). 
 * If unavailable, falls back to spawning 'sw_vers'.
 *
 * @param out_buf  Destination buffer for the distro string.
 * @param buf_size Maximum size of the destination buffer.
 */
static void sys_get_distro(char *out_buf, const size_t buf_size);

/**
 * Maps the current Darwin release version to a macOS marketing name.
 * 
 * @return Pointer to a static string containing the OS name.
 */
static const char* sys_get_os_name(void);

/**
 * Retrieves the hardware model name of the device.
 * On ARM64, it first attempts to get a "pretty" marketing name from IORegistry 
 * (AppleARMPE product description). If that fails or if on Intel, it falls 
 * back to the standard "hw.model" sysctl.
 *
 * @param out_buf  Destination buffer for the model name string.
 * @param buf_size Maximum size of the destination buffer.
 */
static void sys_get_model_name(char *out_buf, size_t buf_size);

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_9
/**
 * Fast plist parser using memory mapping.
 * Opens a file, maps it to the process address space, and creates a 
 * CFPropertyListRef without copying the underlying data.
 *
 * @param path Path to the .plist file.
 * @return A CFPropertyListRef object, or NULL if the file cannot be read or parsed.
 */
static CFPropertyListRef create_plist_from_file(const char* path);
#endif

/**
 * Formats the raw system uptime into a human-readable duration string.
 * Calculates duration by subtracting kern.boottime from current time.
 *
 * @param out_buf  Destination buffer for the formatted string.
 * @param buf_size Maximum size of the destination buffer.
 */
static void sys_format_uptime(char *out_buf, const size_t buf_size);

/**
 * Retrieves the total count of active processes in the system.
 * Uses a KERN_PROC_ALL sysctl snapshot.
 *
 * @param out_buf  Destination buffer for the count string.
 * @param buf_size Maximum size of the destination buffer.
 */
static void sys_get_procs_count(char *out_buf, const size_t buf_size);

void system_init(void) {
    const char *fallback = "unknown";
    memset(&sys_info, 0, sizeof(struct utsname));

    if (uname(&sys_info) != 0) {
        V_PRINTF("[Error] uname() failed: %s\n", strerror(errno));

        strncpy(sys_info.sysname,  fallback, sizeof(sys_info.sysname)  - 1);
        strncpy(sys_info.nodename, fallback, sizeof(sys_info.nodename) - 1);
        strncpy(sys_info.release,  fallback, sizeof(sys_info.release)  - 1);
        strncpy(sys_info.machine,  fallback, sizeof(sys_info.machine)  - 1);
    }
}

void system_print_header(void) {
    char username[MAXLOGNAME] = {0};

    sys_get_identity(username, MAXLOGNAME);
    ui_render_header(username, sys_info.nodename);
}

void system_print_info(void) {
    char os_buf[LINE_BUFFER] = {0};
    sys_get_distro(os_buf, LINE_BUFFER);
    
    char device_model[LINE_BUFFER] = {0};
    sys_get_model_name(device_model, LINE_BUFFER);

    char uptime_buf[LINE_BUFFER] = {0};
    sys_format_uptime(uptime_buf, LINE_BUFFER);

    char procs_buf[SMALL_BUFFER] = {0};
    sys_get_procs_count(procs_buf, SMALL_BUFFER);

    ui_print_info("OS", os_buf);
    ui_print_info("Device", device_model);
    ui_print_info("Kernel", sys_info.release);
    ui_print_info("Uptime", uptime_buf);
    ui_print_info("Processes", procs_buf);
}

static void sys_get_identity(char *out_buf, const size_t buf_size) {
    if (!out_buf || buf_size == 0) {
        V_PRINTF("[Error] %s: invalid arguments\n", __func__);
        return;
    }

    struct passwd *pwd;
    uid_t uid = geteuid();

    if ((pwd = getpwuid(uid)) == NULL) {
        V_PRINTF("[Error] getpwuid(%u) failed: %s\n", uid, strerror(errno));
        snprintf(out_buf, buf_size, "unknown");
        return;
    }

    snprintf(out_buf, buf_size, "%s", pwd->pw_name);
}

static void sys_get_distro(char *out_buf, const size_t buf_size) {
    if (!out_buf || buf_size == 0) {
        V_PRINTF("[Error] %s: invalid arguments\n", __func__);
        return;
    }

    char product_version[SMALL_BUFFER] = {0};
    size_t size = sizeof(product_version);

    snprintf(out_buf, buf_size, "%s ", sys_get_os_name());
    size_t current_len = strlen(out_buf);

    /* Try modern sysctl first (macOS 10.13.4+) */
    if (sysctlbyname("kern.osproductversion", product_version, &size, NULL, 0) == 0) {
        snprintf(out_buf + current_len, buf_size - current_len, "%s (%s)", 
                 product_version, sys_info.machine);
        return;
    }

    /* Fallback to sw_vers for older systems */
    if (capture_line("sw_vers -productVersion", product_version, sizeof(product_version)) != 0) {
        V_PRINTF("[Error] failed to capture sw_vers output\n");
        strncpy(product_version, "unknown", sizeof(product_version) - 1);
    }

    snprintf(out_buf + current_len, buf_size - current_len, "%s (%s)", 
             product_version, sys_info.machine);
} 

static const char* sys_get_os_name(void) {
    int kern_major_number = atoi(sys_info.release);

    for (int i = 0; darwin_map[i].name != NULL; i++) {
        if (darwin_map[i].major == kern_major_number) {
            return darwin_map[i].name;
        }
    }

    return "macOS";
}

static void sys_get_model_name(char *out_buf, size_t buf_size) {
    if (!out_buf || buf_size == 0) {
        V_PRINTF("[Error] %s: invalid arguments\n", __func__);
        return;
    }

    size_t size = buf_size;
    if (sysctlbyname("hw.model", out_buf, &size, NULL, 0) != 0) {
        V_PRINTF("[Error] sysctlbyname(hw.model) failed: %s\n", strerror(errno));
        snprintf(out_buf, buf_size, "Unknown Apple Device");
        return;
    }

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_9
    const char* path = "/System/Library/PrivateFrameworks/ServerInformation.framework/Versions/A/Resources/en.lproj/SIMachineAttributes.plist";
    
    CFPropertyListRef plist = create_plist_from_file(path);
    
    if (!plist) {
        /* Fallback for different localized folder naming conventions */
        path = "/System/Library/PrivateFrameworks/ServerInformation.framework/Versions/A/Resources/English.lproj/SIMachineAttributes.plist";
        V_PRINTF("[Info] %s: trying fallback path: %s\n", __func__, path);
        plist = create_plist_from_file(path);
    }

    if (!plist) {
        V_PRINTF("[Error] %s: SIMachineAttributes.plist not found or failed to parse\n", __func__);
        return; 
    }

    if (CFGetTypeID(plist) == CFDictionaryGetTypeID()) {
        CFDictionaryRef main_dict = (CFDictionaryRef)plist;

        CFStringRef model_key = CFStringCreateWithCString(kCFAllocatorDefault, out_buf, kCFStringEncodingUTF8);
        if (model_key) {
            CFDictionaryRef model_dict = CFDictionaryGetValue(main_dict, model_key);

            if (model_dict && CFGetTypeID(model_dict) == CFDictionaryGetTypeID()) {
                CFDictionaryRef local_dict = CFDictionaryGetValue(model_dict, CFSTR("_LOCALIZABLE_"));
                
                if (local_dict && CFGetTypeID(local_dict) == CFDictionaryGetTypeID()) {
                    CFStringRef marketing_name = CFDictionaryGetValue(local_dict, CFSTR("marketingModel"));

                    if (marketing_name && CFGetTypeID(marketing_name) == CFStringGetTypeID()) {
                        CFStringGetCString(marketing_name, out_buf, buf_size, kCFStringEncodingUTF8);
                    } else {
                        V_PRINTF("[Warning] %s: 'marketingModel' key missing for %s\n", __func__, out_buf);
                    }
                } else {
                    V_PRINTF("[Warning] %s: '_LOCALIZABLE_' dictionary missing for %s\n", __func__, out_buf);
                }
            } else {
                V_PRINTF("[Warning] %s: Model ID %s not found in attributes database\n", __func__, out_buf);
            }

            CFRelease(model_key);
        }
    } else {
        V_PRINTF("[Error] %s: Root of plist is not a dictionary\n", __func__);
    }

    CFRelease(plist);
#endif
}

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_9
static CFPropertyListRef create_plist_from_file(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        return NULL;
    }

    if (sb.st_size == 0) {
        V_PRINTF("[Error] create_plist_from_file(%s): file is empty\n", path);
        close(fd);
        return NULL;
    }

    void *ptr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED) {
        close(fd);
        return NULL;
    }

    CFDataRef data_ref = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, 
                                                    ptr, 
                                                    sb.st_size, 
                                                    kCFAllocatorNull);

    CFPropertyListRef plist = CFPropertyListCreateWithData(kCFAllocatorDefault, 
                                                           data_ref, 
                                                           kCFPropertyListImmutable, 
                                                           NULL, 
                                                           NULL);

    CFRelease(data_ref);
    munmap(ptr, sb.st_size);
    close(fd);

    return plist;
}
#endif

static void sys_format_uptime(char *out_buf, const size_t buf_size) {
    if (!out_buf || buf_size == 0) {
        V_PRINTF("[Error] %s: invalid arguments\n", __func__);
        return;
    }

    struct timeval boot_time = {0, 0};
    size_t size = sizeof(boot_time);

    if (sysctlbyname("kern.boottime", &boot_time, &size, NULL, 0) != 0) {
        V_PRINTF("[Error] sysctlbyname(kern.boottime) failed: %s\n", strerror(errno));
        return;
    }

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_VERSION_11_0
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        V_PRINTF("[Error] time() failed: %s\n", strerror(errno));
        return;
    }

    time_t uptime = now - boot_time.tv_sec;
    if (uptime < 0) uptime = 0;

    time_t days = uptime / 86400;
    uptime %= 86400;
    time_t hours = uptime / 3600;
    uptime %= 3600;
    time_t minutes = uptime / 60;
    time_t seconds = uptime % 60;

    if (days > 0) {
        snprintf(out_buf, buf_size, "%ld days, %02ld:%02ld:%02ld", 
                 (long)days, (long)hours, (long)minutes, (long)seconds);
    } else {
        snprintf(out_buf, buf_size, "%02ld:%02ld:%02ld", 
                 (long)hours, (long)minutes, (long)seconds);
    }
#else
    ino_t uptime = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
    if (uptime == 0) {
        V_PRINTF("[Error] clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) failed: %s\n", strerror(errno));
        return;
    }

    uptime /= NANOSECONDS_IN_SECONDS;

    ino_t days = uptime / 86400;
    uptime %= 86400;
    ino_t hours = uptime / 3600;
    uptime %= 3600;
    ino_t minutes = uptime / 60;
    ino_t seconds = uptime % 60;

    if (days > 0) {
        snprintf(out_buf, buf_size, "%llu days, %02llu:%02llu:%02llu", 
                 (unsigned long long)days, (unsigned long long)hours, 
                 (unsigned long long)minutes, (unsigned long long)seconds);
    } else {
        snprintf(out_buf, buf_size, "%02llu:%02llu:%02llu",
                 (unsigned long long)hours, (unsigned long long)minutes, 
                 (unsigned long long)seconds);
    }
#endif
}

static void sys_get_procs_count(char *out_buf, const size_t buf_size) {
    if (!out_buf || buf_size == 0) {
        V_PRINTF("[Error] %s: invalid arguments\n", __func__);
        return;
    }

    int mib[3] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL};
    size_t len;

    /* Initialize with fallback */
    snprintf(out_buf, buf_size, "-");

    /* First call to get the required buffer size */
    if (sysctl(mib, 3, NULL, &len, NULL, 0) < 0) {
        V_PRINTF("[Error] sysctl(KERN_PROC_ALL) size query failed: %s\n", strerror(errno));
        return;
    }

    struct kinfo_proc *procs = malloc(len);
    if (!procs) {
        V_PRINTF("[Error] malloc(%zu) for kinfo_proc failed: %s\n", len, strerror(errno));
        return;
    }

    /* Second call to fetch the actual process list */
    if (sysctl(mib, 3, procs, &len, NULL, 0) < 0) {
        V_PRINTF("[Error] sysctl(KERN_PROC_ALL) data fetch failed: %s\n", strerror(errno));
        free(procs);
        return;
    }

    size_t count = len / sizeof(struct kinfo_proc);
    snprintf(out_buf, buf_size, "%zu", count);

    free(procs);
}