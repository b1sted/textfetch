/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <mntent.h>
#include <sys/statvfs.h>

#include "pal/hardware_os.h"
#include "test_framework.h"
#include "test_helpers.h"

/* Include the translation unit to exercise its static helpers directly. */
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../src/platform/linux-common/hardware.c"

static void test_valid_mount(void) {
    struct mntent entry;

    memset(&entry, 0, sizeof(entry));
    entry.mnt_fsname = "/dev/sda1";
    entry.mnt_dir = "/";
    entry.mnt_type = "ext4";
    TEST_CHECK(is_valid_mount(&entry));

    entry.mnt_fsname = "/dev/mapper/vg-root";
    entry.mnt_dir = "/home";
    TEST_CHECK(is_valid_mount(&entry));

    entry.mnt_fsname = "/dev/loop0";
    TEST_CHECK(!is_valid_mount(&entry));

    entry.mnt_fsname = "tmpfs";
    TEST_CHECK(!is_valid_mount(&entry));

    entry.mnt_fsname = "/dev/sda1";
    entry.mnt_type = "fuse";
    TEST_CHECK(!is_valid_mount(&entry));

    entry.mnt_type = "erofs";
    TEST_CHECK(!is_valid_mount(&entry));

    entry.mnt_type = "ext4";
    entry.mnt_dir = "/boot";
    TEST_CHECK(!is_valid_mount(&entry));

    entry.mnt_dir = "/bootloader";
    TEST_CHECK(!is_valid_mount(&entry));

    entry.mnt_dir = "/run/user/1000";
    TEST_CHECK(!is_valid_mount(&entry));

    entry.mnt_dir = "/var/lib";
    TEST_CHECK(!is_valid_mount(&entry));

    entry.mnt_dir = "/mnt/wslg/distro";
    TEST_CHECK(!is_valid_mount(&entry));

    entry.mnt_dir = "/apex";
    TEST_CHECK(!is_valid_mount(&entry));

    entry.mnt_dir = "/bootstrap-apex";
    TEST_CHECK(!is_valid_mount(&entry));

    entry.mnt_dir = "/.";
    TEST_CHECK(!is_valid_mount(&entry));

    entry.mnt_dir = "/mnt/data";
    TEST_CHECK(is_valid_mount(&entry));
}

static void test_print_disk(void) {
    struct statvfs fs;
    struct mntent entry;
    test_capture_t capture;

    memset(&fs, 0, sizeof(fs));
    memset(&entry, 0, sizeof(entry));
    entry.mnt_type = "ext4";

    fs.f_frsize = 4096;
    fs.f_blocks = 1000;
    fs.f_bfree = 500;

    TEST_CHECK(test_capture_begin(&capture));
    hw_print_disk("/test", &fs, &entry);
    char *output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "Disk (/test): 1.95 MiB / 3.91 MiB (50%) [ext4]\n");
    free(output);

    fs.f_frsize = 0;
    fs.f_blocks = 0;
    fs.f_bfree = 0;
    entry.mnt_type = "xfs";

    TEST_CHECK(test_capture_begin(&capture));
    hw_print_disk("/empty", &fs, &entry);
    output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "Disk (/empty): 0.00 B / 0.00 B (0%) [xfs]\n");
    free(output);
}

static void test_print_memory(void) {
    mem_info_t node = {
        .ram_size = 1024ULL * 1024ULL * 1024ULL,
        .ram_used = 512ULL * 1024ULL * 1024ULL,
        .swap_size = 2ULL * 1024ULL * 1024ULL * 1024ULL,
        .swap_used = 0,
    };
    test_capture_t capture;

    TEST_CHECK(test_capture_begin(&capture));
    hw_print_mem_info(MEM_RAM, &node);
    char *output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "RAM: 512.00 MiB / 1.00 GiB (50%)\n");
    free(output);

    TEST_CHECK(test_capture_begin(&capture));
    hw_print_mem_info(MEM_SWAP, &node);
    output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "Swap: 0.00 B / 2.00 GiB (0%)\n");
    free(output);

    TEST_CHECK(test_capture_begin(&capture));
    hw_print_mem_info((mem_flags_t)(MEM_RAM | MEM_SWAP), &node);
    output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output,
                "RAM: 512.00 MiB / 1.00 GiB (50%)\n"
                "Swap: 0.00 B / 2.00 GiB (0%)\n");
    free(output);

    TEST_CHECK(test_capture_begin(&capture));
    hw_print_mem_info((mem_flags_t)0, &node);
    output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    TEST_STR_EQ(output, "");
    free(output);
}

static void test_meminfo_parsing(void) {
    char without_available[] =
        "MemTotal:       1000 kB\n"
        "MemFree:         100 kB\n"
        "Buffers:          50 kB\n"
        "Cached:          200 kB\n"
        "SReclaimable:     50 kB\n"
        "SwapTotal:       500 kB\n"
        "SwapFree:        200 kB\n";
    mem_info_t node;
    mem_flags_t flags = 0;
    memset(&node, 0, sizeof(node));

    FILE *fp = fmemopen(without_available, strlen(without_available), "r");
    TEST_CHECK(fp != NULL);
    if (fp) {
        hw_parse_meminfo(fp, &node, &flags);
        fclose(fp);
    }

    TEST_CHECK((flags & MEM_RAM) != 0);
    TEST_CHECK(node.ram_size == 1000ULL * BYTES_PER_KIB);
    TEST_CHECK(node.ram_used == 600ULL * BYTES_PER_KIB);
    TEST_CHECK((flags & MEM_SWAP) != 0);
    TEST_CHECK(node.swap_used == 300ULL * BYTES_PER_KIB);

    char with_available[] =
        "MemTotal:       1000 kB\n"
        "MemAvailable:    250 kB\n"
        "MemFree:         100 kB\n"
        "Buffers:          50 kB\n"
        "Cached:          200 kB\n";
    memset(&node, 0, sizeof(node));
    flags = 0;

    fp = fmemopen(with_available, strlen(with_available), "r");
    TEST_CHECK(fp != NULL);
    if (fp) {
        hw_parse_meminfo(fp, &node, &flags);
        fclose(fp);
    }

    TEST_CHECK((flags & MEM_RAM) != 0);
    TEST_CHECK(node.ram_used == 750ULL * BYTES_PER_KIB);
}

static void test_actual_memory(void) {
    test_capture_t capture;

    TEST_CHECK(test_capture_begin(&capture));
    hw_get_mem_info();
    char *output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    TEST_STR_CONTAINS(output, "RAM: ");
    free(output);
}

static void test_actual_drives(void) {
    test_capture_t capture;

    TEST_CHECK(test_capture_begin(&capture));
    hw_get_drives_info();
    char *output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    if (output[0] != '\0') TEST_STR_CONTAINS(output, "Disk (");
    free(output);
}

int main(void) {
    test_valid_mount();
    test_print_disk();
    test_print_memory();
    test_meminfo_parsing();
    test_actual_memory();
    test_actual_drives();

    return test_finish("linux_common_hardware");
}
