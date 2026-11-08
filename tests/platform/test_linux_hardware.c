/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../support/test_framework.h"
#include "../support/test_helpers.h"

/* Include the translation unit to exercise its static helpers directly. */
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../../src/platform/linux/hardware.c"

static void check_cpu_name(const char *input, const char *expected)
{
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s", input);
    hw_sanitize_cpu_name(buffer);
    TEST_STR_EQ(buffer, expected);
}

static void test_sanitize_cpu_name(void)
{
    check_cpu_name("Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz", "Intel Core i7-8700");
    check_cpu_name("12th Gen Intel(R) Core(TM) i7-12700K", "Intel Core i7-12700K");
    check_cpu_name("AMD Ryzen 7 5800X 8-Core Processor", "AMD Ryzen 7 5800X");
    check_cpu_name("AMD Ryzen 5 5600X 6-Core Processor", "AMD Ryzen 5 5600X");
    check_cpu_name("Intel Core i5-8250U CPU @ 1.60GHz", "Intel Core i5-8250U");
    check_cpu_name("Intel(R) Xeon(R) CPU E5-2697 v2 @ 2.70GHz", "Intel Xeon E5-2697 v2");
    check_cpu_name("Intel(R) Xeon(R) CPU @ 2.20GHz", "Intel Xeon");
    check_cpu_name("Intel(R) Atom(TM) CPU N450 @ 1.66GHz", "Intel Atom N450");
    check_cpu_name("AMD APU", "AMD");
    check_cpu_name("Intel Core i7-8700 Processor", "Intel Core i7-8700");
    check_cpu_name("Intel Core i7-8700 with Radeon Graphics", "Intel Core i7-8700");
    check_cpu_name("Apple M1", "Apple M1");
    check_cpu_name("Unknown", "Unknown");
    check_cpu_name("", "");

    hw_sanitize_cpu_name(NULL);
}

static tree_forest_t *make_pci_forest(void)
{
    tree_forest_t *ids = tree_create_forest(2);
    if (!ids) {
        return NULL;
    }

    tree_node_t *nvidia = tree_create_node(0x10de, "NVIDIA");
    tree_add_child(nvidia, 0x1c82, "GeForce GTX 1050 Ti");
    tree_add_child(nvidia, 0x0de1, "GeForce GT 630");
    tree_add_to_forest(ids, nvidia);

    tree_node_t *amd = tree_create_node(0x1002, "AMD");
    tree_add_child(amd, 0x73ff, "Navi 23");
    tree_add_to_forest(ids, amd);

    return ids;
}

static void test_gpu_lookup(void)
{
    char buffer[256] = {0};
    gpu_node_t *list = NULL;

    list = add_gpu(list, 0x10de, 0x1c82);
    TEST_CHECK(list != NULL);
    TEST_INT_EQ(list->impl_id, 0x10de);
    TEST_INT_EQ(list->part_id, 0x1c82);
    TEST_CHECK(list->next == NULL);

    tree_forest_t *ids = make_pci_forest();
    TEST_CHECK(ids != NULL);
    memset(buffer, 0, sizeof(buffer));
    hw_gpu_lookup_names(buffer, sizeof(buffer), ids, list);
    TEST_STR_EQ(buffer, "NVIDIA GeForce GTX 1050 Ti [0x1C82]");
    tree_destroy_forest(ids);
    free_gpu_list(list);

    list = add_gpu(NULL, 0x10de, 0x1c82);
    list = add_gpu(list, 0x10de, 0x1c82);
    ids = make_pci_forest();
    memset(buffer, 0, sizeof(buffer));
    hw_gpu_lookup_names(buffer, sizeof(buffer), ids, list);
    TEST_STR_EQ(buffer, "2 x NVIDIA GeForce GTX 1050 Ti [0x1C82]");
    tree_destroy_forest(ids);
    free_gpu_list(list);

    list = add_gpu(NULL, 0x10de, 0x9999);
    ids = make_pci_forest();
    memset(buffer, 0, sizeof(buffer));
    hw_gpu_lookup_names(buffer, sizeof(buffer), ids, list);
    TEST_STR_EQ(buffer, "NVIDIA [0x9999]");
    tree_destroy_forest(ids);
    free_gpu_list(list);

    list = add_gpu(NULL, 0x1234, 0x5678);
    memset(buffer, 0, sizeof(buffer));
    hw_gpu_lookup_names(buffer, sizeof(buffer), NULL, list);
    TEST_STR_EQ(buffer, "Generic GPU [0x5678]");
    free_gpu_list(list);

    list = add_gpu(NULL, 0x1234, 0x5678);
    char small[8] = {0};
    hw_gpu_lookup_names(small, sizeof(small), NULL, list);
    TEST_CHECK(strlen(small) < sizeof(small));
    TEST_CHECK(small[sizeof(small) - 1] == '\0');
    free_gpu_list(list);

    list = add_gpu(NULL, 0x1234, 0x5678);
    ids = make_pci_forest();
    hw_gpu_lookup_names(NULL, 0, ids, list);
    tree_destroy_forest(ids);
    free_gpu_list(list);

    list = add_gpu(NULL, 0x1234, 0x5678);
    ids = make_pci_forest();
    hw_gpu_lookup_names(buffer, sizeof(buffer), ids, list);
    tree_destroy_forest(ids);
    free_gpu_list(list);

    free_gpu_list(NULL);
}

static void test_battery_metrics(void)
{
    bat_data_t data = {0};

    hw_battery_read_metrics("textfetch-missing-battery", &data);

    TEST_STR_EQ(data.model, "");
    TEST_STR_EQ(data.capacity, "");
    TEST_STR_EQ(data.status, "");
    TEST_CHECK((data.health & HEALTH_UNKNOWN) != 0);
}

static void test_hardware_lists(void)
{
    hw_node_t *elements = add_element(NULL, "BAT0");
    TEST_CHECK(elements != NULL);
    TEST_STR_EQ(elements->name, "BAT0");
    TEST_CHECK(elements->next == NULL);

    elements = add_element(elements, "BAT1");
    TEST_CHECK(elements != NULL);
    TEST_STR_EQ(elements->name, "BAT1");
    TEST_CHECK(elements->next != NULL);
    TEST_STR_EQ(elements->next->name, "BAT0");

    char long_name[100];
    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    elements = add_element(elements, long_name);
    TEST_CHECK(strlen(elements->name) == SMALL_BUFFER - 1);
    free_data_list(elements);

    free_data_list(NULL);
}

static bool fake_sysfs_write(const char *path, const char *value)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return false;
    }

    bool written = (fputs(value, fp) >= 0);
    if (fclose(fp) != 0) {
        written = false;
    }

    return written;
}

static bool fake_sysfs_add_cpu(const char *cpu_dir, const char *cpu_name, const char *package_id,
                               const char *core_id, const char *max_freq)
{
    char path[PATH_BUFFER];

    snprintf(path, sizeof(path), "%s%s", cpu_dir, cpu_name);
    if (mkdir(path, 0700) != 0) {
        return false;
    }

    snprintf(path, sizeof(path), "%s%s/topology", cpu_dir, cpu_name);
    if (mkdir(path, 0700) != 0) {
        return false;
    }

    snprintf(path, sizeof(path), "%s%s/topology/physical_package_id", cpu_dir, cpu_name);
    if (!fake_sysfs_write(path, package_id)) {
        return false;
    }

    snprintf(path, sizeof(path), "%s%s/topology/core_id", cpu_dir, cpu_name);
    if (!fake_sysfs_write(path, core_id)) {
        return false;
    }

    if (max_freq == NULL) {
        return true;
    }

    snprintf(path, sizeof(path), "%s%s/cpufreq", cpu_dir, cpu_name);
    if (mkdir(path, 0700) != 0) {
        return false;
    }

    snprintf(path, sizeof(path), "%s%s/cpufreq/cpuinfo_max_freq", cpu_dir, cpu_name);
    return fake_sysfs_write(path, max_freq);
}

static bool fake_sysfs_add_cpufreq_attr(const char *cpu_dir, const char *cpu_name, const char *attr,
                                        const char *value)
{
    char path[PATH_BUFFER * 2];

    snprintf(path, sizeof(path), "%s%s/cpufreq", cpu_dir, cpu_name);
    if (mkdir(path, 0700) != 0 && errno != EEXIST) {
        return false;
    }

    snprintf(path, sizeof(path), "%s%s/cpufreq/%s", cpu_dir, cpu_name, attr);
    return fake_sysfs_write(path, value);
}

static void fake_sysfs_remove_cpufreq_attr(const char *cpu_dir, const char *cpu_name,
                                           const char *attr)
{
    char path[PATH_BUFFER * 2];

    snprintf(path, sizeof(path), "%s%s/cpufreq/%s", cpu_dir, cpu_name, attr);
    unlink(path);

    snprintf(path, sizeof(path), "%s%s/cpufreq", cpu_dir, cpu_name);
    rmdir(path);
}

static void fake_sysfs_remove_cpu(const char *cpu_dir, const char *cpu_name, bool has_topology,
                                  bool has_freq)
{
    char path[PATH_BUFFER];

    if (has_freq) {
        snprintf(path, sizeof(path), "%s%s/cpufreq/cpuinfo_max_freq", cpu_dir, cpu_name);
        unlink(path);

        snprintf(path, sizeof(path), "%s%s/cpufreq", cpu_dir, cpu_name);
        rmdir(path);
    }

    if (has_topology) {
        snprintf(path, sizeof(path), "%s%s/topology/physical_package_id", cpu_dir, cpu_name);
        unlink(path);

        snprintf(path, sizeof(path), "%s%s/topology/core_id", cpu_dir, cpu_name);
        unlink(path);

        snprintf(path, sizeof(path), "%s%s/topology", cpu_dir, cpu_name);
        rmdir(path);
    }

    snprintf(path, sizeof(path), "%s%s", cpu_dir, cpu_name);
    rmdir(path);
}

static void test_sparse_cpu_packages(void)
{
    char template[] = "/tmp/textfetch-cpu-XXXXXX";
    char *root = mkdtemp(template);
    TEST_CHECK(root != NULL);
    if (!root) {
        return;
    }

    char cpu_dir[PATH_BUFFER];
    snprintf(cpu_dir, sizeof(cpu_dir), "%s/", root);

    TEST_CHECK(fake_sysfs_add_cpu(cpu_dir, "cpu0", "0", "1", "3200000"));
    TEST_CHECK(fake_sysfs_add_cpu(cpu_dir, "cpu1", "0", "2", "2800000"));
    TEST_CHECK(fake_sysfs_add_cpu(cpu_dir, "cpu2", "2", "0", NULL));
    TEST_CHECK(fake_sysfs_add_cpu(cpu_dir, "cpu3", "2", "1", "4000000"));

    /* Offline CPU with no topology directory must be skipped, not defaulted. */
    char offline_path[PATH_BUFFER + 16];
    snprintf(offline_path, sizeof(offline_path), "%scpu4", cpu_dir);
    TEST_CHECK(mkdir(offline_path, 0700) == 0);

    char online_path[PATH_BUFFER + 32];
    snprintf(online_path, sizeof(online_path), "%scpu4/online", cpu_dir);
    TEST_CHECK(fake_sysfs_write(online_path, "0\n"));

    cpu_data_t cpus[MAX_CPU_PACKAGES];
    uint16_t cpu_map[MAX_CPU_PACKAGES];
    uint8_t packages = 0;

    memset(cpus, 0, sizeof(cpus));
    memset(cpu_map, 0xFF, sizeof(cpu_map));

    hw_parse_cpu_topology(cpu_dir, cpus, cpu_map, &packages);

    /* Packages 0 and 2 exist, so package 1 must remain unused. */
    TEST_INT_EQ(packages, 2);
    TEST_CHECK(cpus[0].exists);
    TEST_CHECK(!cpus[1].exists);
    TEST_CHECK(cpus[2].exists);
    TEST_CHECK(!cpus[3].exists);

    TEST_INT_EQ(cpu_map[0], 0);
    TEST_INT_EQ(cpu_map[1], 0xFFFF);
    TEST_INT_EQ(cpu_map[2], 2);

    TEST_INT_EQ(set_count_bits(&(bitset_t){.bits = cpus[0].core_bits, .capacity = MAX_CPU_CORES},
                               CORES_SET_BLOCKS),
                2);
    TEST_INT_EQ(set_count_bits(&(bitset_t){.bits = cpus[2].core_bits, .capacity = MAX_CPU_CORES},
                               CORES_SET_BLOCKS),
                2);
    TEST_INT_EQ(cpus[0].threads, 2);
    TEST_INT_EQ(cpus[2].threads, 2);

    /* The peak frequency must come from the fastest core in each package. */
    TEST_CHECK(cpus[0].frequency > 3.19 && cpus[0].frequency < 3.21);
    TEST_CHECK(cpus[2].frequency > 3.99 && cpus[2].frequency < 4.01);

    unlink(online_path);
    fake_sysfs_remove_cpu(cpu_dir, "cpu4", false, false);
    fake_sysfs_remove_cpu(cpu_dir, "cpu3", true, true);
    fake_sysfs_remove_cpu(cpu_dir, "cpu2", true, false);
    fake_sysfs_remove_cpu(cpu_dir, "cpu1", true, true);
    fake_sysfs_remove_cpu(cpu_dir, "cpu0", true, true);
    rmdir(root);
}

static void test_smt_cpu_threads(void)
{
    char template[] = "/tmp/textfetch-cpu-smt-XXXXXX";
    char *root = mkdtemp(template);
    TEST_CHECK(root != NULL);
    if (!root) {
        return;
    }

    char cpu_dir[PATH_BUFFER];
    snprintf(cpu_dir, sizeof(cpu_dir), "%s/", root);

    /* Two physical cores exposed as four logical SMT threads. */
    TEST_CHECK(fake_sysfs_add_cpu(cpu_dir, "cpu0", "0", "0", NULL));
    TEST_CHECK(fake_sysfs_add_cpu(cpu_dir, "cpu1", "0", "0", NULL));
    TEST_CHECK(fake_sysfs_add_cpu(cpu_dir, "cpu2", "0", "1", NULL));
    TEST_CHECK(fake_sysfs_add_cpu(cpu_dir, "cpu3", "0", "1", NULL));

    cpu_data_t cpus[MAX_CPU_PACKAGES];
    uint16_t cpu_map[MAX_CPU_PACKAGES];
    uint8_t packages = 0;

    memset(cpus, 0, sizeof(cpus));
    memset(cpu_map, 0xFF, sizeof(cpu_map));

    hw_parse_cpu_topology(cpu_dir, cpus, cpu_map, &packages);

    TEST_INT_EQ(packages, 1);
    TEST_INT_EQ(set_count_bits(&(bitset_t){.bits = cpus[0].core_bits, .capacity = MAX_CPU_CORES},
                               CORES_SET_BLOCKS),
                2);
    TEST_INT_EQ(cpus[0].threads, 4);

    snprintf(cpus[0].model, sizeof(cpus[0].model), "%s", "SMT CPU");

    char formatted[LINE_BUFFER];
    TEST_CHECK(hw_format_cpu_package(&cpus[0], formatted, sizeof(formatted)));
    TEST_STR_EQ(formatted, "SMT CPU (2C / 4T)");

    fake_sysfs_remove_cpu(cpu_dir, "cpu3", true, false);
    fake_sysfs_remove_cpu(cpu_dir, "cpu2", true, false);
    fake_sysfs_remove_cpu(cpu_dir, "cpu1", true, false);
    fake_sysfs_remove_cpu(cpu_dir, "cpu0", true, false);
    rmdir(root);
}

static void test_online_map_parser(void)
{
    char template[] = "/tmp/textfetch-cpu-online-map-XXXXXX";
    char *root = mkdtemp(template);
    TEST_CHECK(root != NULL);
    if (!root) {
        return;
    }

    char cpu_dir[PATH_BUFFER];
    snprintf(cpu_dir, sizeof(cpu_dir), "%s/", root);

    char path[PATH_BUFFER * 2];
    snprintf(path, sizeof(path), "%sonline", cpu_dir);
    TEST_CHECK(fake_sysfs_write(path, "0-2,4,8-9\n"));

    cpu_online_map_t map;
    memset(&map, 0, sizeof(map));
    hw_cpu_read_online_map(cpu_dir, &map);

    TEST_CHECK(map.valid);
    TEST_INT_EQ(map.count, 3);
    TEST_INT_EQ(map.ranges[0].first, 0);
    TEST_INT_EQ(map.ranges[0].last, 2);
    TEST_INT_EQ(map.ranges[1].first, 4);
    TEST_INT_EQ(map.ranges[1].last, 4);
    TEST_INT_EQ(map.ranges[2].first, 8);
    TEST_INT_EQ(map.ranges[2].last, 9);

    TEST_CHECK(fake_sysfs_write(path, "0-\n"));
    hw_cpu_read_online_map(cpu_dir, &map);
    TEST_CHECK(!map.valid);
    TEST_INT_EQ(map.count, 0);

    unlink(path);
    rmdir(root);
}

static void test_online_map_filters_cpus(void)
{
    char template[] = "/tmp/textfetch-cpu-online-filter-XXXXXX";
    char *root = mkdtemp(template);
    TEST_CHECK(root != NULL);
    if (!root) {
        return;
    }

    char cpu_dir[PATH_BUFFER];
    snprintf(cpu_dir, sizeof(cpu_dir), "%s/", root);

    TEST_CHECK(fake_sysfs_add_cpu(cpu_dir, "cpu0", "0", "0", "3000000"));
    TEST_CHECK(fake_sysfs_add_cpu(cpu_dir, "cpu1", "0", "1", "2000000"));
    TEST_CHECK(fake_sysfs_add_cpu(cpu_dir, "cpu2", "0", "2", "1000000"));

    char path[PATH_BUFFER * 2];
    snprintf(path, sizeof(path), "%sonline", cpu_dir);
    TEST_CHECK(fake_sysfs_write(path, "0,2\n"));

    /* The map must win over a stale per-CPU online attribute. */
    snprintf(path, sizeof(path), "%scpu1/online", cpu_dir);
    TEST_CHECK(fake_sysfs_write(path, "1\n"));

    cpu_data_t cpus[MAX_CPU_PACKAGES];
    uint16_t cpu_map[MAX_CPU_PACKAGES];
    uint8_t packages = 0;

    memset(cpus, 0, sizeof(cpus));
    memset(cpu_map, 0xFF, sizeof(cpu_map));

    hw_parse_cpu_topology(cpu_dir, cpus, cpu_map, &packages);

    TEST_INT_EQ(packages, 1);
    TEST_CHECK(cpus[0].exists);
    TEST_INT_EQ(set_count_bits(&(bitset_t){.bits = cpus[0].core_bits, .capacity = MAX_CPU_CORES},
                               CORES_SET_BLOCKS),
                2);
    TEST_INT_EQ(cpus[0].threads, 2);
    TEST_CHECK(cpus[0].frequency > 2.99 && cpus[0].frequency < 3.01);

    unlink(path);
    snprintf(path, sizeof(path), "%sonline", cpu_dir);
    unlink(path);

    fake_sysfs_remove_cpu(cpu_dir, "cpu2", true, true);
    fake_sysfs_remove_cpu(cpu_dir, "cpu1", true, true);
    fake_sysfs_remove_cpu(cpu_dir, "cpu0", true, true);
    rmdir(root);
}

static void test_uniprocessor_without_topology(void)
{
    char template[] = "/tmp/textfetch-cpu-uni-XXXXXX";
    char *root = mkdtemp(template);
    TEST_CHECK(root != NULL);
    if (!root) {
        return;
    }

    char cpu_dir[PATH_BUFFER];
    snprintf(cpu_dir, sizeof(cpu_dir), "%s/", root);

    char cpu0_path[PATH_BUFFER + 16];
    snprintf(cpu0_path, sizeof(cpu0_path), "%scpu0", cpu_dir);
    TEST_CHECK(mkdir(cpu0_path, 0700) == 0);

    cpu_data_t cpus[MAX_CPU_PACKAGES];
    uint16_t cpu_map[MAX_CPU_PACKAGES];
    uint8_t packages = 0;

    memset(cpus, 0, sizeof(cpus));
    memset(cpu_map, 0xFF, sizeof(cpu_map));

    hw_parse_cpu_topology(cpu_dir, cpus, cpu_map, &packages);

    /* A topology-less online cpu0 must still be detected as package 0. */
    TEST_INT_EQ(packages, 1);
    TEST_CHECK(cpus[0].exists);
    TEST_INT_EQ(cpu_map[0], 0);
    TEST_INT_EQ(set_count_bits(&(bitset_t){.bits = cpus[0].core_bits, .capacity = MAX_CPU_CORES},
                               CORES_SET_BLOCKS),
                1);
    TEST_INT_EQ(cpus[0].threads, 1);

    /* No cpufreq nodes: the model must survive and print without a suffix. */
    snprintf(cpus[0].model, sizeof(cpus[0].model), "%s", "ARM Cortex-A7");
    hw_get_cpu_freq(cpu_dir, cpus, 0, "cpu0");

    TEST_STR_EQ(cpus[0].model, "ARM Cortex-A7");
    TEST_CHECK(cpus[0].frequency == 0.0);

    char formatted[LINE_BUFFER];
    TEST_CHECK(hw_format_cpu_package(&cpus[0], formatted, sizeof(formatted)));
    TEST_STR_EQ(formatted, "ARM Cortex-A7 (1C / 1T)");
    TEST_CHECK(strstr(formatted, "@") == NULL);

    rmdir(cpu0_path);
    rmdir(root);
}

static void test_current_freq_fallback(void)
{
    char template[] = "/tmp/textfetch-cpu-cur-XXXXXX";
    char *root = mkdtemp(template);
    TEST_CHECK(root != NULL);
    if (!root) {
        return;
    }

    char cpu_dir[PATH_BUFFER];
    snprintf(cpu_dir, sizeof(cpu_dir), "%s/", root);

    TEST_CHECK(fake_sysfs_add_cpu(cpu_dir, "cpu0", "0", "0", NULL));

    /* Fixed-clock kernels expose only the current frequency. */
    TEST_CHECK(fake_sysfs_add_cpufreq_attr(cpu_dir, "cpu0", "scaling_cur_freq", "1104000"));

    cpu_data_t cpus[MAX_CPU_PACKAGES];
    uint16_t cpu_map[MAX_CPU_PACKAGES];
    uint8_t packages = 0;

    memset(cpus, 0, sizeof(cpus));
    memset(cpu_map, 0xFF, sizeof(cpu_map));

    hw_parse_cpu_topology(cpu_dir, cpus, cpu_map, &packages);

    TEST_INT_EQ(packages, 1);
    TEST_CHECK(cpus[0].exists);
    TEST_CHECK(cpus[0].frequency > 1.103 && cpus[0].frequency < 1.105);

    snprintf(cpus[0].model, sizeof(cpus[0].model), "%s", "ARM Cortex-A7");

    char formatted[LINE_BUFFER];
    TEST_CHECK(hw_format_cpu_package(&cpus[0], formatted, sizeof(formatted)));
    TEST_STR_EQ(formatted, "ARM Cortex-A7 (1C / 1T) @ 1.104 GHz");

    fake_sysfs_remove_cpufreq_attr(cpu_dir, "cpu0", "scaling_cur_freq");
    fake_sysfs_remove_cpu(cpu_dir, "cpu0", true, false);
    rmdir(root);
}

static void test_policy_current_freq_fallback(void)
{
    char template[] = "/tmp/textfetch-cpu-policy-XXXXXX";
    char *root = mkdtemp(template);
    TEST_CHECK(root != NULL);
    if (!root) {
        return;
    }

    char cpu_dir[PATH_BUFFER];
    snprintf(cpu_dir, sizeof(cpu_dir), "%s/", root);

    TEST_CHECK(fake_sysfs_add_cpu(cpu_dir, "cpu0", "0", "0", NULL));

    char path[PATH_BUFFER * 2];
    snprintf(path, sizeof(path), "%scpufreq", cpu_dir);
    TEST_CHECK(mkdir(path, 0700) == 0);

    snprintf(path, sizeof(path), "%scpufreq/policy0", cpu_dir);
    TEST_CHECK(mkdir(path, 0700) == 0);

    snprintf(path, sizeof(path), "%scpufreq/policy0/scaling_cur_freq", cpu_dir);
    TEST_CHECK(fake_sysfs_write(path, "1104000"));

    cpu_data_t cpus[MAX_CPU_PACKAGES];
    uint16_t cpu_map[MAX_CPU_PACKAGES];
    uint8_t packages = 0;

    memset(cpus, 0, sizeof(cpus));
    memset(cpu_map, 0xFF, sizeof(cpu_map));

    hw_parse_cpu_topology(cpu_dir, cpus, cpu_map, &packages);

    TEST_INT_EQ(packages, 1);
    TEST_CHECK(cpus[0].exists);
    TEST_CHECK(cpus[0].frequency > 1.103 && cpus[0].frequency < 1.105);

    snprintf(path, sizeof(path), "%scpufreq/policy0/scaling_cur_freq", cpu_dir);
    unlink(path);

    snprintf(path, sizeof(path), "%scpufreq/policy0", cpu_dir);
    rmdir(path);

    snprintf(path, sizeof(path), "%scpufreq", cpu_dir);
    rmdir(path);

    fake_sysfs_remove_cpu(cpu_dir, "cpu0", true, false);
    rmdir(root);
}

static void test_cpu_fallback_without_nodes(void)
{
    char template[] = "/tmp/textfetch-cpu-empty-XXXXXX";
    char *root = mkdtemp(template);
    TEST_CHECK(root != NULL);
    if (!root) {
        return;
    }

    char cpu_dir[PATH_BUFFER];
    snprintf(cpu_dir, sizeof(cpu_dir), "%s/", root);

    cpu_data_t cpus[MAX_CPU_PACKAGES];
    uint16_t cpu_map[MAX_CPU_PACKAGES];
    uint8_t packages = 0;

    memset(cpus, 0, sizeof(cpus));
    memset(cpu_map, 0xFF, sizeof(cpu_map));

    hw_parse_cpu_topology(cpu_dir, cpus, cpu_map, &packages);

    /* /proc/cpuinfo is readable, so the single-core fallback must kick in. */
    TEST_INT_EQ(packages, 1);
    TEST_CHECK(cpus[0].exists);
    TEST_INT_EQ(cpu_map[0], 0);
    TEST_INT_EQ(set_count_bits(&(bitset_t){.bits = cpus[0].core_bits, .capacity = MAX_CPU_CORES},
                               CORES_SET_BLOCKS),
                1);

    rmdir(root);
}

static void test_actual_cpu(void)
{
    cpu_data_t cpus[MAX_CPU_PACKAGES];
    uint16_t cpu_map[MAX_CPU_PACKAGES];
    uint8_t packages = 0;

    memset(cpus, 0, sizeof(cpus));
    memset(cpu_map, 0xFF, sizeof(cpu_map));

    hw_parse_cpu_topology(SYS_CPU_DIR, cpus, cpu_map, &packages);
    TEST_CHECK(packages > 0);
    TEST_CHECK(packages <= MAX_CPU_PACKAGES);

    hw_get_cpu_model(cpus, cpu_map);
    TEST_CHECK(strlen(cpus[0].model) > 0);
    TEST_CHECK(cpus[0].frequency >= 0.0);

    test_capture_t capture = {0};
    TEST_CHECK(test_capture_begin(&capture));
    hw_get_cpu_info();
    char *output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    TEST_STR_CONTAINS(output, "CPU: ");
    free(output);
}

static void test_actual_hardware(void)
{
    test_capture_t capture = {0};
    char *output = NULL;

    TEST_CHECK(test_capture_begin(&capture));
    hw_get_gpu_info();
    output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    free(output);

    TEST_CHECK(test_capture_begin(&capture));
    hw_get_bat_info();
    output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    free(output);

    gpu_node_t *gpus = hw_get_all_gpus();
    free_gpu_list(gpus);

    hw_node_t *batteries = hw_get_all_batteries();
    free_data_list(batteries);
}

int main(void)
{
    test_sanitize_cpu_name();
    test_gpu_lookup();
    test_battery_metrics();
    test_hardware_lists();
    test_sparse_cpu_packages();
    test_smt_cpu_threads();
    test_online_map_parser();
    test_online_map_filters_cpus();
    test_uniprocessor_without_topology();
    test_current_freq_fallback();
    test_policy_current_freq_fallback();
    test_cpu_fallback_without_nodes();
    test_actual_cpu();
    test_actual_hardware();

    return test_finish("linux_hardware");
}
