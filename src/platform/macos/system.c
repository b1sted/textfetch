/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <time.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <CoreFoundation/CoreFoundation.h>

#include "capture.h"
#include "defs.h"
#include "sys_utils.h"

#include "pal/system_os.h"

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_VERSION_11_0
#define NANOSECONDS_IN_SECONDS 1000000000ULL
#endif

/**
 * Mapping structure to associate Darwin kernel major versions
 * with macOS marketing/release names.
 */
typedef struct {
    int major;
    const char *name;
} macos_name_map_t;

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
 * Maps the current Darwin release version to a macOS marketing name.
 *
 * @return Pointer to a static string containing the OS name.
 */
static const char *sys_get_os_name(void);

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_9
/**
 * Fast plist parser using memory mapping.
 * Opens a file, maps it to the process address space, and creates a
 * CFPropertyListRef without copying the underlying data.
 *
 * @param path Path to the .plist file.
 * @return A CFPropertyListRef object, or NULL if the file cannot be read or
 * parsed.
 */
static CFPropertyListRef create_plist_from_file(const char *path);
#endif

/**
 * Formats the raw system uptime into a human-readable duration string.
 * Calculates duration by subtracting kern.boottime from current time.
 *
 * @param out_buf  Destination buffer for the formatted string.
 * @param buf_size Maximum size of the destination buffer.
 */
static void sys_get_uptime();

/**
 * Retrieves the total count of active processes in the system.
 * Uses a KERN_PROC_ALL sysctl snapshot.
 *
 * @param out_buf  Destination buffer for the count string.
 * @param buf_size Maximum size of the destination buffer.
 */
static void sys_get_procs_count();

void system_init(void) {
    const char *fallback = "unknown";
    static struct utsname uts;

    bool uname_ok = (uname(&uts) == 0);
    if (!uname_ok) V_PRINTF("Error: uname failed: %s\n", strerror(errno));

    snprintf(sys_data.sysname, sizeof(sys_data.sysname), 
             "%s", uname_ok ? uts.sysname : fallback);
    snprintf(sys_data.nodename, sizeof(sys_data.nodename), 
             "%s", uname_ok ? uts.nodename : fallback);
    snprintf(sys_data.release, sizeof(sys_data.release), 
             "%s", uname_ok ? uts.release : fallback);
    snprintf(sys_data.machine, sizeof(sys_data.machine), 
             "%s", uname_ok ? uts.machine : fallback);

    sys_get_uptime();
    sys_get_procs_count();
}

void sys_get_distro(char *out_buf, const size_t buf_size) {
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
        snprintf(out_buf + current_len, buf_size - current_len, 
                 "%s (%s)", product_version, sys_data.machine);
        return;
    }

    /* Fallback to sw_vers for older systems */
    if (capture_line("sw_vers", "-productVersion", product_version, sizeof(product_version)) != 0) {
        V_PRINTF("[Error] failed to capture sw_vers output\n");
        strncpy(product_version, "unknown", sizeof(product_version) - 1);
    }

    snprintf(out_buf + current_len, buf_size - current_len, 
             "%s (%s)", product_version, sys_data.machine);
}

static const char *sys_get_os_name(void) {
    int kern_major_number = atoi(sys_data.release);

    for (int i = 0; darwin_map[i].name != NULL; i++) {
        if (darwin_map[i].major == kern_major_number) {
            return darwin_map[i].name;
        }
    }

    return "macOS";
}

void sys_get_model_name(char *out_buf, size_t buf_size) {
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
    const char *path = "/System/Library/PrivateFrameworks/ServerInformation.framework/"
                       "Versions/A/Resources/en.lproj/SIMachineAttributes.plist";

    CFPropertyListRef plist = create_plist_from_file(path);

    if (!plist) {
        /* Fallback for different localized folder naming conventions */
        path = "/System/Library/PrivateFrameworks/ServerInformation.framework/"
               "Versions/A/Resources/English.lproj/SIMachineAttributes.plist";
        V_PRINTF("[Info] %s: trying fallback path: %s\n", __func__, path);
        plist = create_plist_from_file(path);
    }

    if (!plist) {
        V_PRINTF("[Error] %s: SIMachineAttributes.plist not found or failed to parse\n",
                 __func__);
        return;
    }

    if (CFGetTypeID(plist) == CFDictionaryGetTypeID()) {
        CFDictionaryRef main_dict = (CFDictionaryRef)plist;

        CFStringRef model_key =
            CFStringCreateWithCString(kCFAllocatorDefault, out_buf, kCFStringEncodingUTF8);
        if (model_key) {
            CFDictionaryRef model_dict = CFDictionaryGetValue(main_dict, model_key);

            if (model_dict && CFGetTypeID(model_dict) == CFDictionaryGetTypeID()) {
                CFDictionaryRef local_dict =
                    CFDictionaryGetValue(model_dict, CFSTR("_LOCALIZABLE_"));

                if (local_dict && CFGetTypeID(local_dict) == CFDictionaryGetTypeID()) {
                    CFStringRef marketing_name =
                        CFDictionaryGetValue(local_dict, CFSTR("marketingModel"));

                    if (marketing_name && 
                        CFGetTypeID(marketing_name) == CFStringGetTypeID()) {
                        CFStringGetCString(marketing_name, out_buf, buf_size,
                                           kCFStringEncodingUTF8);
                    } else {
                        V_PRINTF("[Warning] %s: 'marketingModel' key missing for %s\n",
                                 __func__, out_buf);
                    }
                } else {
                    V_PRINTF("[Warning] %s: '_LOCALIZABLE_' dictionary missing for %s\n",
                             __func__, out_buf);
                }
            } else {
                V_PRINTF("[Warning] %s: Model ID %s not found in attributes database\n",
                         __func__, out_buf);
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
static CFPropertyListRef create_plist_from_file(const char *path) {
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

    CFDataRef data_ref =
        CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, ptr, sb.st_size,
                                    kCFAllocatorNull);

    CFPropertyListRef plist = CFPropertyListCreateWithData(kCFAllocatorDefault, data_ref,
                                                           kCFPropertyListImmutable, NULL, 
                                                           NULL);

    CFRelease(data_ref);
    munmap(ptr, sb.st_size);
    close(fd);

    return plist;
}
#endif

static void sys_get_uptime() {
    sys_data.uptime = 0;

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_VERSION_11_0
    uint64_t uptime_ns = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
    if (uptime_ns == 0) {
        V_PRINTF("[ERROR] clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) failed: %s\n",
                 strerror(errno));
        return;
    }

    sys_data.uptime = (uptime_ns / NANOSECONDS_IN_SECONDS);
#else
    struct timeval boottime = {0, 0};
    size_t size = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};

    if (sysctl(mib, 2, &boottime, &size, NULL, 0) == 0) {
        struct timeval now;
        gettimeofday(&now, NULL);

        if (now.tv_sec >= boottime.tv_sec) {
            sys_data.uptime = (uint64_t)(now.tv_sec - boottime.tv_sec);
        }
    } else {
        V_PRINTF("[ERROR] sysctl(KERN_BOOTTIME) failed: %s\n", strerror(errno));
    }
#endif
}

static void sys_get_procs_count() {
    int mib[3] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL};
    size_t len;

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

    sys_data.procs = (uint32_t)(len / sizeof(struct kinfo_proc));
    free(procs);
}