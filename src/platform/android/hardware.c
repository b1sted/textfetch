/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <inttypes.h>

#include <unistd.h>

#include <sys/system_properties.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "defs.h"
#include "sys_utils.h"
#include "ui.h"

#include "pal/hardware_os.h"

/**
 * Determines the SoC model name using Android system properties.
 *
 * @param node Pointer to the CPU info structure.
 */
static void hw_get_cpu_model(cpu_info_t *node);

/**
 * Checks the OS sysconf limits to find the active processor count.
 *
 * @param node Pointer to the CPU info structure.
 */
static void hw_get_cpu_cores(cpu_info_t *node);

/**
 * Reads scaling frequency from sysfs for CPU0.
 *
 * @param node Pointer to the CPU info structure.
 */
static void hw_get_cpu_freq(cpu_info_t *node);

void hw_get_cpu_info(void) {
    cpu_info_t node;
    memset(&node, 0, sizeof(cpu_info_t));

    hw_get_cpu_model(&node);
    hw_get_cpu_cores(&node);
    hw_get_cpu_freq(&node);

    char cpu_info[LINE_BUFFER] = {0};
    snprintf(cpu_info, sizeof(cpu_info), "%s%s%s", node.model, node.cores, node.frequency);

    ui_print_info("CPU", cpu_info);
}

static void hw_get_cpu_model(cpu_info_t *node) {
    if (__system_property_get("ro.soc.model", node->model) <= 0) {
        if (__system_property_get("ro.board.platform", node->model) <= 0) {
            V_PRINTF("[WARNING] Could not read cpu name, using 'Unknown'\n");
            snprintf(node->model, sizeof(node->model), "Unknown");
        }
    }
}

static void hw_get_cpu_cores(cpu_info_t *node) {
    long cores = sysconf(_SC_NPROCESSORS_CONF);
    if (cores == -1) {
        V_PRINTF("[WARNING] Could not get cpu cores number: %s\n", strerror(errno));
        cores = 0;
    }

    snprintf(node->cores, sizeof(node->cores), " (%ld)", cores);
}

static void hw_get_cpu_freq(cpu_info_t *node) {
    uint32_t freq_khz = 0;
    util_read_uint32("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", &freq_khz);

    double freq_ghz = (double)freq_khz / KHZ_PER_GHZ;
    snprintf(node->frequency, sizeof(node->frequency), " @ %.03f GHz", freq_ghz);
}

void hw_get_gpu_info(void) {
    EGLDisplay egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major_version, minor_version;

    if (!eglInitialize(egl_display, &major_version, &minor_version)) {
        return;
    }

    const EGLint config_attributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_NONE
    };

    eglBindAPI(EGL_OPENGL_ES_API);

    EGLint num_configs;
    EGLConfig egl_config;

    if (!eglChooseConfig(egl_display, config_attributes, &egl_config, 1, &num_configs) ||
        !num_configs) {
        V_PRINTF("[ERROR]: Failed to choose EGL config: %04x\n", eglGetError());
        eglTerminate(egl_display);
        return;
    }

    const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLContext egl_context = eglCreateContext(egl_display, egl_config,
                                              EGL_NO_CONTEXT, context_attributes);
    if (egl_context == EGL_NO_CONTEXT) {
        V_PRINTF("[ERROR]: Failed to create EGL context: %04x\n", eglGetError());
        eglTerminate(egl_display);
        return;
    }

    const EGLint surface_attributes[] = {
        EGL_WIDTH,  1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };

    EGLSurface egl_surface = eglCreatePbufferSurface(egl_display, egl_config,
                                                     surface_attributes);
    if (egl_surface == EGL_NO_SURFACE) {
        V_PRINTF("[ERROR]: Failed to create pbuffer surface: %04x\n", eglGetError());
        eglDestroyContext(egl_display, egl_context);
        eglTerminate(egl_display);
        return;
    }

    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    ui_print_info("GPU", (const char *)glGetString(GL_RENDERER));

    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(egl_display, egl_surface);
    eglDestroyContext(egl_display, egl_context);
    eglTerminate(egl_display);
}

/*
 * NOTE: Battery parsing is skipped on non-rooted Android systems.
 * 1. Direct access to /sys/class/power_supply/ is blocked by Google since
 * Android 10+ to prevent battery-based fingerprinting (privacy protection).
 * 2. 'termux-api' bridge is too slow (~300-500ms overhead) for a utility
 *    aiming for sub-50ms execution time.
 * 3. 'dumpsys' requires android.permission.DUMP (not available by default).
 */
void hw_get_bat_info(void) {
    if (!util_is_file_exist("/sys/class/power_supply/battery/present"))
        return;

    uint8_t capacity = 0;
    char status[MINI_BUFFER] = {0};
    char health[MINI_BUFFER] = {0};

    util_read_uint8("/sys/class/power_supply/battery/capacity", &capacity);
    util_read_line("/sys/class/power_supply/battery/status", status, sizeof(status));
    util_read_line("/sys/class/power_supply/battery/health", health, sizeof(health));

    char information[LINE_BUFFER] = {0};
    snprintf(information, sizeof(information),
             "%" PRIu8 "(%s, Health: %s)", capacity, status, health);
    ui_print_info("Battery", information);
}
