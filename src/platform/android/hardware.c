/* SPDX-License-Identifier: MIT */

#include "pal/hardware_os.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <inttypes.h>

#include <dirent.h>
#include <unistd.h>

#include <sys/system_properties.h>

#if defined(__ANDROID__)
#include <dlfcn.h>
#endif

#include "defs.h"
#include "sys_utils.h"
#include "ui.h"

#if defined(__ANDROID__)
/*
 * Minimal subset of the Vulkan ABI used for GPU discovery.
 *
 * The definitions are kept local so that textfetch can load the Android system
 * Vulkan loader at runtime. This avoids build-time Vulkan headers and a
 * link-time dependency on Mesa/libglvnd, both of which are unavailable in a
 * stock Termux installation.
 */
typedef int32_t VkResult;
typedef uint32_t VkFlags;
typedef struct VkInstance_T *VkInstance;
typedef struct VkPhysicalDevice_T *VkPhysicalDevice;

typedef struct {
    int32_t sType;
    const void *pNext;
    const char *pApplicationName;
    uint32_t applicationVersion;
    const char *pEngineName;
    uint32_t engineVersion;
    uint32_t apiVersion;
} VkApplicationInfo;

typedef struct {
    int32_t sType;
    const void *pNext;
    VkFlags flags;
    const VkApplicationInfo *pApplicationInfo;
    uint32_t enabledLayerCount;
    const char *const *ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char *const *ppEnabledExtensionNames;
} VkInstanceCreateInfo;

/*
 * The driver writes a full VkPhysicalDeviceProperties structure (824 bytes on
 * 64-bit platforms), while only the leading fields are read here. The union
 * keeps the storage wide enough and properly aligned for the driver.
 */
typedef union {
    uint64_t alignment;
    unsigned char bytes[1024];
} vk_properties_t;

#define VK_SUCCESS 0
#define VK_INCOMPLETE 5
#define VK_STRUCTURE_TYPE_APPLICATION_INFO 0
#define VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 1
#define VK_API_VERSION_1_0 (1u << 22)
#define VK_DEVICE_NAME_OFFSET 20
#define VK_MAX_PHYSICAL_DEVICES 8

typedef VkResult (*vk_create_instance_t)(const VkInstanceCreateInfo *, const void *, VkInstance *);
typedef void (*vk_destroy_instance_t)(VkInstance, const void *);
typedef VkResult (*vk_enumerate_physical_devices_t)(VkInstance, uint32_t *, VkPhysicalDevice *);
typedef void (*vk_get_physical_device_properties_t)(VkPhysicalDevice, void *);

/**
 * Loads the Android system Vulkan loader.
 *
 * @return A dlopen handle, or NULL when the library cannot be loaded.
 */
static void *hw_open_system_vulkan(void);

/**
 * Reads the Vulkan device name of the first available physical device.
 *
 * @param out_buf Buffer to store the device name.
 * @param buf_size Maximum size of the buffer.
 * @return 1 on success, 0 otherwise.
 */
static int hw_read_vulkan_renderer(char *out_buf, size_t buf_size);

/**
 * Derives a generic GPU vendor name from Android system properties.
 *
 * Used as a fallback when Vulkan is unavailable.
 *
 * @param out_buf Buffer to store the GPU name.
 * @param buf_size Maximum size of the buffer.
 */
static void hw_read_gpu_property(char *out_buf, size_t buf_size);
#endif /* __ANDROID__ */

/**
 * Determines the SoC model name using Android system properties.
 *
 * @param cpu Pointer to the CPU info structure.
 */
static void hw_get_cpu_model(cpu_info_t *cpu);

/**
 * Checks the OS sysconf limits to find the active processor count.
 *
 * @param cpu Pointer to the CPU info structure.
 */
static void hw_get_cpu_cores(cpu_info_t *cpu);

/**
 * Reads the highest available frequency from a cpufreq policy directory.
 *
 * @param policy_path Absolute path to a cpufreq policy directory.
 * @return The maximum frequency in kHz, or 0 when no source is available.
 */
static uint32_t hw_read_policy_max_freq(const char *policy_path);

/**
 * Scans all cpufreq policy clusters and reports the highest maximum frequency.
 *
 * @param cpu Pointer to the CPU info structure.
 */
static void hw_get_cpu_freq(cpu_info_t *cpu);

void hw_get_cpu_info(void)
{
    cpu_info_t cpu = {0};

    hw_get_cpu_model(&cpu);
    hw_get_cpu_cores(&cpu);
    hw_get_cpu_freq(&cpu);

    char cpu_info[LINE_BUFFER] = {0};
    snprintf(cpu_info, sizeof(cpu_info), "%s%s%s", cpu.model, cpu.cores, cpu.frequency);

    ui_print_info("CPU", cpu_info);
}

static void hw_get_cpu_model(cpu_info_t *cpu)
{
    if (__system_property_get("ro.soc.model", cpu->model) <= 0) {
        if (__system_property_get("ro.board.platform", cpu->model) <= 0) {
            V_PRINTF("[WARNING] Could not read CPU name, using 'Unknown'\n");
            snprintf(cpu->model, sizeof(cpu->model), "Unknown");
        }
    }
}

static void hw_get_cpu_cores(cpu_info_t *cpu)
{
    int64_t cores = (int64_t)sysconf(_SC_NPROCESSORS_CONF);
    if (cores == -1) {
        V_PRINTF("[WARNING] Could not get CPU cores number: %s\n", strerror(errno));
        cores = 0;
    }

    snprintf(cpu->cores, sizeof(cpu->cores), " (%" PRId64 ")", cores);
}

static uint32_t hw_read_policy_max_freq(const char *policy_path)
{
    static const char *const freq_files[] = {
        "cpuinfo_max_freq",
        "scaling_max_freq",
        "scaling_cur_freq",
        "cpuinfo_cur_freq",
    };

    char freq_path[PATH_BUFFER] = {0};

    for (size_t i = 0; i < sizeof(freq_files) / sizeof(freq_files[0]); i++) {
        uint32_t freq_khz = 0;

        snprintf(freq_path, sizeof(freq_path), "%s/%s", policy_path, freq_files[i]);

        if (util_read_uint32(freq_path, &freq_khz) && freq_khz > 0) {
            return freq_khz;
        }
    }

    return 0;
}

static void hw_get_cpu_freq(cpu_info_t *cpu)
{
    static const char *const cpufreq_dir = "/sys/devices/system/cpu/cpufreq";

    char policy_path[PATH_BUFFER] = {0};
    uint32_t max_freq_khz = 0;

    cpu->frequency[0] = '\0';

    DIR *dir = opendir(cpufreq_dir);
    if (dir) {
        struct dirent *entry = NULL;

        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "policy", 6) != 0) {
                continue;
            }
            if (!util_is_numeric_string(entry->d_name + 6)) {
                continue;
            }

            snprintf(policy_path, sizeof(policy_path), "%s/%s", cpufreq_dir, entry->d_name);

            uint32_t freq_khz = hw_read_policy_max_freq(policy_path);
            if (freq_khz > max_freq_khz) {
                max_freq_khz = freq_khz;
            }
        }

        closedir(dir);
    }

    if (max_freq_khz == 0) {
        max_freq_khz = hw_read_policy_max_freq("/sys/devices/system/cpu/cpu0/cpufreq");
    }

    if (max_freq_khz > 0) {
        snprintf(cpu->frequency, sizeof(cpu->frequency), " @ %.03f GHz",
                 (double)max_freq_khz / KHZ_PER_GHZ);
    }
}

void hw_get_gpu_info(void)
{
#if !defined(__ANDROID__)
    return;
#else
    char gpu_name[LINE_BUFFER] = {0};

    if (!hw_read_vulkan_renderer(gpu_name, sizeof(gpu_name))) {
        hw_read_gpu_property(gpu_name, sizeof(gpu_name));
    }

    if (gpu_name[0] != '\0') {
        ui_print_info("GPU", gpu_name);
    } else {
        V_PRINTF("[WARNING] Could not determine GPU model\n");
    }
#endif
}

#if defined(__ANDROID__)
static void *hw_open_system_vulkan(void)
{
    static const char *const paths[] = {
        "/system/lib64/libvulkan.so",
        "/system/lib/libvulkan.so",
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        void *handle = dlopen(paths[i], RTLD_NOW | RTLD_LOCAL);
        if (handle != NULL) {
            return handle;
        }
    }

    const char *error = dlerror();
    V_PRINTF("[ERROR] Failed to load Android Vulkan: %s\n",
             error != NULL ? error : "unknown error");

    return NULL;
}

static int hw_read_vulkan_renderer(char *out_buf, size_t buf_size)
{
    int success = 0;
    void *vulkan_lib = NULL;
    VkInstance instance = NULL;
    VkPhysicalDevice devices[VK_MAX_PHYSICAL_DEVICES] = {0};
    vk_properties_t properties;
    uint32_t device_count = 0;

    memset(&properties, 0, sizeof(properties));

    vk_create_instance_t create_instance = NULL;
    vk_destroy_instance_t destroy_instance = NULL;
    vk_enumerate_physical_devices_t enumerate_physical_devices = NULL;
    vk_get_physical_device_properties_t get_physical_device_properties = NULL;

    vulkan_lib = hw_open_system_vulkan();
    if (vulkan_lib == NULL) {
        return 0;
    }

    create_instance = (vk_create_instance_t)dlsym(vulkan_lib, "vkCreateInstance");
    destroy_instance = (vk_destroy_instance_t)dlsym(vulkan_lib, "vkDestroyInstance");
    enumerate_physical_devices =
        (vk_enumerate_physical_devices_t)dlsym(vulkan_lib, "vkEnumeratePhysicalDevices");
    get_physical_device_properties =
        (vk_get_physical_device_properties_t)dlsym(vulkan_lib, "vkGetPhysicalDeviceProperties");

    if (create_instance == NULL || destroy_instance == NULL || enumerate_physical_devices == NULL ||
        get_physical_device_properties == NULL) {
        V_PRINTF("[ERROR] Android Vulkan loader is missing required symbols\n");
        goto cleanup;
    }

    const VkApplicationInfo application_info = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO,
        NULL,
        "textfetch",
        1,
        "textfetch",
        1,
        VK_API_VERSION_1_0,
    };
    const VkInstanceCreateInfo create_info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &application_info, 0, NULL, 0, NULL,
    };

    VkResult result = create_instance(&create_info, NULL, &instance);
    if (result != VK_SUCCESS) {
        V_PRINTF("[ERROR] Failed to create an Android Vulkan instance: %" PRId32 "\n", result);
        goto cleanup;
    }

    result = enumerate_physical_devices(instance, &device_count, NULL);
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || device_count == 0) {
        V_PRINTF("[ERROR] No Android Vulkan physical devices found\n");
        goto cleanup;
    }

    if (device_count > VK_MAX_PHYSICAL_DEVICES) {
        device_count = VK_MAX_PHYSICAL_DEVICES;
    }

    result = enumerate_physical_devices(instance, &device_count, devices);
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || device_count == 0) {
        V_PRINTF("[ERROR] Failed to enumerate Android Vulkan physical devices\n");
        goto cleanup;
    }

    get_physical_device_properties(devices[0], &properties);

    const char *device_name = (const char *)(properties.bytes + VK_DEVICE_NAME_OFFSET);
    if (device_name[0] != '\0') {
        snprintf(out_buf, buf_size, "%s", device_name);
        success = 1;
    } else {
        V_PRINTF("[WARNING] Android Vulkan returned an empty device name\n");
    }

cleanup:
    if (instance != NULL && destroy_instance != NULL) {
        destroy_instance(instance, NULL);
    }
    if (vulkan_lib != NULL) {
        dlclose(vulkan_lib);
    }

    return success;
}

static void hw_read_gpu_property(char *out_buf, size_t buf_size)
{
    static const struct {
        const char *value;
        const char *name;
    } vendors[] = {
        {"mali", "ARM Mali"},
        {"adreno", "Qualcomm Adreno"},
        {"powervr", "PowerVR"},
        {"swiftshader", "Google SwiftShader"},
    };

    char property[PROP_VALUE_MAX] = {0};

    if (__system_property_get("ro.hardware.vulkan", property) <= 0) {
        if (__system_property_get("ro.hardware.egl", property) <= 0) {
            return;
        }
    }

    for (size_t i = 0; i < sizeof(vendors) / sizeof(vendors[0]); i++) {
        if (strcmp(property, vendors[i].value) == 0) {
            snprintf(out_buf, buf_size, "%s", vendors[i].name);
            return;
        }
    }
}
#endif /* __ANDROID__ */

/*
 * NOTE: Battery parsing is skipped on non-rooted Android systems.
 * 1. Direct access to /sys/class/power_supply/ is blocked by Google since
 * Android 10+ to prevent battery-based fingerprinting (privacy protection).
 * 2. 'termux-api' bridge is too slow (~300-500ms overhead) for a utility
 *    aiming for sub-50ms execution time.
 * 3. 'dumpsys' requires android.permission.DUMP (not available by default).
 */
void hw_get_bat_info(void)
{
    if (!util_is_file_exist("/sys/class/power_supply/battery/present")) {
        return;
    }

    uint8_t capacity = 0;
    char status[MINI_BUFFER] = {0};
    char health[MINI_BUFFER] = {0};

    util_read_uint8("/sys/class/power_supply/battery/capacity", &capacity);
    util_read_line("/sys/class/power_supply/battery/status", status, sizeof(status));
    util_read_line("/sys/class/power_supply/battery/health", health, sizeof(health));

    if (status[0] == '\0') {
        snprintf(status, sizeof(status), "%s", "Unknown");
    }

    if (health[0] == '\0') {
        snprintf(health, sizeof(health), "%s", "Unknown");
    }

    char information[LINE_BUFFER] = {0};
    snprintf(information, sizeof(information), "%" PRIu8 "%% (%s, Health: %s)", capacity, status,
             health);
    ui_print_info("Battery", information);
}
