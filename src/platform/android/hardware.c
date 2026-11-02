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
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#endif

#include "defs.h"
#include "sys_utils.h"
#include "ui.h"

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
    EGLDisplay egl_display = EGL_NO_DISPLAY;
    EGLContext egl_context = EGL_NO_CONTEXT;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLConfig egl_config = 0;
    EGLint major_version = 0;
    EGLint minor_version = 0;
    EGLint num_configs = 0;

    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display == EGL_NO_DISPLAY) {
        goto cleanup;
    }

    if (!eglInitialize(egl_display, &major_version, &minor_version)) {
        goto cleanup;
    }

    const EGLint config_attributes[] = {EGL_RENDERABLE_TYPE,
                                        EGL_OPENGL_ES2_BIT,
                                        EGL_SURFACE_TYPE,
                                        EGL_PBUFFER_BIT,
                                        EGL_RED_SIZE,
                                        8,
                                        EGL_GREEN_SIZE,
                                        8,
                                        EGL_BLUE_SIZE,
                                        8,
                                        EGL_ALPHA_SIZE,
                                        8,
                                        EGL_NONE};

    eglBindAPI(EGL_OPENGL_ES_API);

    if (!eglChooseConfig(egl_display, config_attributes, &egl_config, 1, &num_configs) ||
        !num_configs) {
        V_PRINTF("[ERROR] Failed to choose EGL config: %04x\n", eglGetError());
        goto cleanup;
    }

    const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

    egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attributes);
    if (egl_context == EGL_NO_CONTEXT) {
        V_PRINTF("[ERROR] Failed to create EGL context: %04x\n", eglGetError());
        goto cleanup;
    }

    const EGLint surface_attributes[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};

    egl_surface = eglCreatePbufferSurface(egl_display, egl_config, surface_attributes);
    if (egl_surface == EGL_NO_SURFACE) {
        V_PRINTF("[ERROR] Failed to create pbuffer surface: %04x\n", eglGetError());
        goto cleanup;
    }

    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    const char *gl_renderer = (const char *)glGetString(GL_RENDERER);
    if (!gl_renderer) {
        V_PRINTF("[WARNING] Could not get GPU name, using 'Unknown'\n");
        gl_renderer = "Unknown";
    }

    ui_print_info("GPU", gl_renderer);

    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

cleanup:
    if (egl_surface != EGL_NO_SURFACE) {
        eglDestroySurface(egl_display, egl_surface);
    }
    if (egl_context != EGL_NO_CONTEXT) {
        eglDestroyContext(egl_display, egl_context);
    }
    if (egl_display != EGL_NO_DISPLAY) {
        eglTerminate(egl_display);
    }
#endif
}

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
