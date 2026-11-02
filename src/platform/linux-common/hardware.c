/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <mntent.h>

#include <sys/statvfs.h>

#include "defs.h"
#include "hashtable.h"
#include "sys_utils.h"
#include "ui.h"

#include "pal/hardware_os.h"

/* Temporary or pseudo-filesystems to ignore when summarizing disk space. */
#define IGNORE_MOUNT_POINTS { \
    "/.",                     \
    "/boot",                  \
    "/mnt/wslg/distro",       \
    "/run/user",              \
    "/var",                   \
    "/apex",                  \
    "/bootstrap-apex",        \
    NULL                      \
}

/* Path for reading virtual memory usage. */
#define MEMINFO_PATH "/proc/meminfo"

/* Path for reading currently mounted filesystems. */
#define MOUNTS_PATH  "/proc/mounts"

/**
 * Evaluates a mount entry to determine if it should be processed.
 * Filters out non-physical devices, loopbacks, FUSE/EROFS, and specific ignore paths.
 *
 * @param ent Pointer to the mount entry structure from getmntent.
 * @return true if the mount point is a valid physical drive, false if it should be skipped.
 */
static bool is_valid_mount(const struct mntent *ent);

/**
 * Formats and prints disk usage information for a specific mount point.
 *
 * @param mnt The mount point path.
 * @param fs Pointer to the filesystem statistics structure.
 * @param ent Pointer to the mount entry structure.
 */
static void hw_print_disk(const char *mnt, const struct statvfs *fs,
                          const struct mntent *ent);

/**
 * Parses /proc/meminfo data and derives used RAM and swap capacities.
 *
 * @param memory_file Open stream for the memory information file.
 * @param mem_node Pointer to the memory info structure to populate.
 * @param flags Pointer to the flags mask to update.
 */
static void hw_parse_meminfo(FILE *memory_file, mem_info_t *mem_node,
                             mem_flags_t *flags) {
    uint64_t ram_free = 0, swap_free = 0;
    uint64_t mem_free = 0, buffers = 0, cached = 0, sreclaimable = 0;
    bool has_mem_available = false;

    char file_line[LINE_BUFFER];
    while (fgets(file_line, LINE_BUFFER, memory_file)) {
        char *delimiter_ptr = strstr(file_line, ":");
        if (!delimiter_ptr) continue;

        *delimiter_ptr = '\0';
        char *key = file_line;
        char *value = delimiter_ptr + 1;
        char *endptr;

        unsigned long long temp_val = strtoull(value, &endptr, 10);

        if (value == endptr) continue;

        if (strcmp(key, "MemTotal") == 0) {
            mem_node->ram_size = temp_val * BYTES_PER_KIB;
        } else if (strcmp(key, "MemAvailable") == 0) {
            ram_free = temp_val * BYTES_PER_KIB;
            has_mem_available = true;
        } else if (strcmp(key, "MemFree") == 0) {
            mem_free = temp_val * BYTES_PER_KIB;
        } else if (strcmp(key, "Buffers") == 0) {
            buffers = temp_val * BYTES_PER_KIB;
        } else if (strcmp(key, "Cached") == 0) {
            cached = temp_val * BYTES_PER_KIB;
        } else if (strcmp(key, "SReclaimable") == 0) {
            sreclaimable = temp_val * BYTES_PER_KIB;
        } else if (strcmp(key, "SwapTotal") == 0) {
            mem_node->swap_size = temp_val * BYTES_PER_KIB;
        } else if (strcmp(key, "SwapFree") == 0) {
            swap_free = temp_val * BYTES_PER_KIB;
        }
    }

    if (!has_mem_available) {
        ram_free = mem_free + buffers + cached + sreclaimable;
    }

    if (mem_node->ram_size > 0) {
        *flags |= MEM_RAM;
        mem_node->ram_used = (ram_free < mem_node->ram_size)
                             ? mem_node->ram_size - ram_free : 0;
    }

    if (mem_node->swap_size > 0) {
        *flags |= MEM_SWAP;
        mem_node->swap_used = (swap_free < mem_node->swap_size)
                              ? mem_node->swap_size - swap_free : 0;
    }
}

void hw_get_mem_info(void) {
    mem_info_t mem_node;
    memset(&mem_node, 0, sizeof(mem_info_t));

    mem_flags_t flags = 0;

    FILE *memory_file = fopen(MEMINFO_PATH, "r");
    if (!memory_file) {
        V_PRINTF("[ERROR] Failed to open %s: %s\n", MEMINFO_PATH, strerror(errno));
        return;
    }

    hw_parse_meminfo(memory_file, &mem_node, &flags);

    fclose(memory_file);

    hw_print_mem_info(flags, &mem_node);
}

void hw_get_drives_info(void) {
    struct mntent *mnt_entry;
    struct statvfs fs;
    memset(&fs, 0, sizeof(struct statvfs));

    FILE *mounts_file = fopen(MOUNTS_PATH, "r");
    if (!mounts_file) {
        V_PRINTF("[ERROR] Failed to open %s: %s\n", MOUNTS_PATH, strerror(errno));
        return;
    }

    string_set_t *outputted_partitions = strset_create(INITIAL_CAPACITY);

    while ((mnt_entry = getmntent(mounts_file)) != NULL) {
        if (!is_valid_mount(mnt_entry)) continue;

        if (statvfs(mnt_entry->mnt_dir, &fs) != 0) {
            V_PRINTF("[ERROR] statvfs failed for %s: %s\n",
                     mnt_entry->mnt_dir, strerror(errno));
            continue;
        }

        if (fs.f_blocks == 0) continue;

        if (!strset_contains(outputted_partitions, mnt_entry->mnt_fsname)) {
            strset_add(outputted_partitions, mnt_entry->mnt_fsname);
            hw_print_disk(mnt_entry->mnt_dir, &fs, mnt_entry);
        }
    }

    strset_destroy(outputted_partitions);
    fclose(mounts_file);
}

/**
 * Checks whether a mount point matches an ignored path at a component boundary.
 *
 * @param mnt_dir The mount point to evaluate.
 * @param ignore The ignored path prefix.
 * @return true if the mount point is the ignored path or one of its children.
 */
static bool is_ignored_mount_point(const char *mnt_dir, const char *ignore) {
    size_t len = strlen(ignore);

    return strncmp(mnt_dir, ignore, len) == 0 &&
           (mnt_dir[len] == '\0' || mnt_dir[len] == '/');
}

static bool is_valid_mount(const struct mntent *ent) {
    if (strncmp(ent->mnt_fsname, "/dev/", 5) != 0) return false;
    if (strncmp(ent->mnt_fsname, "/dev/loop", 9) == 0) return false;

    if (strncmp(ent->mnt_type, "fuse", 4) == 0 ||
        strcmp(ent->mnt_type, "erofs") == 0) return false;

    const char *ignore_mount_points[] = IGNORE_MOUNT_POINTS;

    for (int i = 0; ignore_mount_points[i] != NULL; i++) {
        if (is_ignored_mount_point(ent->mnt_dir, ignore_mount_points[i])) {
            return false;
        }
    }

    return true;
}

static void hw_print_disk(const char *mnt, const struct statvfs *fs,
                          const struct mntent *ent) {
    uint64_t block_size = fs->f_frsize;
    double total_size   = (uint64_t)fs->f_blocks * block_size;
    double free_size    = fs->f_bfree * block_size;
    double used_size    = total_size - free_size;

    char label[LINE_BUFFER];
    snprintf(label, LINE_BUFFER, "Disk (%s)", mnt);

    char usage_info[LINE_BUFFER];
    size_t usage_len = sizeof(usage_info);

    util_format_size(total_size, used_size, usage_info, usage_len, UNIT_B);

    size_t len = strlen(usage_info);
    snprintf(usage_info + len, usage_len - len, " [%s]", ent->mnt_type);

    ui_print_info(label, usage_info);
}
