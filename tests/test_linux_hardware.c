/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_framework.h"
#include "test_helpers.h"

/* Include the translation unit to exercise its static helpers directly. */
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../src/platform/linux/hardware.c"

static void check_cpu_name(const char *input, const char *expected) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s", input);
    hw_sanitize_cpu_name(buffer);
    TEST_STR_EQ(buffer, expected);
}

static void test_sanitize_cpu_name(void) {
    check_cpu_name("Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz",
                   "Intel Core i7-8700");
    check_cpu_name("12th Gen Intel(R) Core(TM) i7-12700K",
                   "Intel Core i7-12700K");
    check_cpu_name("AMD Ryzen 7 5800X 8-Core Processor",
                   "AMD Ryzen 7 5800X");
    check_cpu_name("AMD Ryzen 5 5600X 6-Core Processor",
                   "AMD Ryzen 5 5600X");
    check_cpu_name("Intel Core i5-8250U CPU @ 1.60GHz",
                   "Intel Core i5-8250U");
    check_cpu_name("Intel(R) Xeon(R) CPU E5-2697 v2 @ 2.70GHz",
                   "Intel Xeon E5-2697 v2");
    check_cpu_name("Intel(R) Xeon(R) CPU @ 2.20GHz", "Intel Xeon");
    check_cpu_name("Intel(R) Atom(TM) CPU N450 @ 1.66GHz",
                   "Intel Atom N450");
    check_cpu_name("AMD APU", "AMD");
    check_cpu_name("Intel Core i7-8700 Processor", "Intel Core i7-8700");
    check_cpu_name("Intel Core i7-8700 with Radeon Graphics",
                   "Intel Core i7-8700");
    check_cpu_name("Apple M1", "Apple M1");
    check_cpu_name("Unknown", "Unknown");
    check_cpu_name("", "");

    hw_sanitize_cpu_name(NULL);
}

static forest *make_pci_forest(void) {
    forest *ids = create_forest(2);
    if (!ids) return NULL;

    node *nvidia = create_node(0x10de, "NVIDIA");
    add_child(nvidia, 0x1c82, "GeForce GTX 1050 Ti");
    add_child(nvidia, 0x0de1, "GeForce GT 630");
    add_tree_to_forest(ids, nvidia);

    node *amd = create_node(0x1002, "AMD");
    add_child(amd, 0x73ff, "Navi 23");
    add_tree_to_forest(ids, amd);

    return ids;
}

static void test_gpu_lookup(void) {
    char buffer[256];
    gpu_node_t *list = NULL;

    list = add_gpu(list, 0x10de, 0x1c82);
    TEST_CHECK(list != NULL);
    TEST_INT_EQ(list->impl_id, 0x10de);
    TEST_INT_EQ(list->part_id, 0x1c82);
    TEST_CHECK(list->next == NULL);

    forest *ids = make_pci_forest();
    TEST_CHECK(ids != NULL);
    memset(buffer, 0, sizeof(buffer));
    hw_gpu_lookup_names(buffer, sizeof(buffer), ids, list);
    TEST_STR_EQ(buffer, "NVIDIA GeForce GTX 1050 Ti [0x1C82]");
    free_gpu_list(list);

    list = add_gpu(NULL, 0x10de, 0x1c82);
    list = add_gpu(list, 0x10de, 0x1c82);
    ids = make_pci_forest();
    memset(buffer, 0, sizeof(buffer));
    hw_gpu_lookup_names(buffer, sizeof(buffer), ids, list);
    TEST_STR_EQ(buffer, "2 x NVIDIA GeForce GTX 1050 Ti [0x1C82]");
    free_gpu_list(list);

    list = add_gpu(NULL, 0x10de, 0x9999);
    ids = make_pci_forest();
    memset(buffer, 0, sizeof(buffer));
    hw_gpu_lookup_names(buffer, sizeof(buffer), ids, list);
    TEST_STR_EQ(buffer, "NVIDIA [0x9999]");
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
    free_gpu_list(list);

    list = add_gpu(NULL, 0x1234, 0x5678);
    ids = make_pci_forest();
    hw_gpu_lookup_names(buffer, sizeof(buffer), ids, list);
    free_gpu_list(list);

    free_gpu_list(NULL);
}

static void test_battery_metrics(void) {
    bat_data_t data;
    memset(&data, 0, sizeof(data));

    hw_battery_read_metrics("textfetch-missing-battery", &data);

    TEST_STR_EQ(data.model, "");
    TEST_STR_EQ(data.capacity, "");
    TEST_STR_EQ(data.status, "");
    TEST_CHECK((data.health & HEALTH_UNKNOWN) != 0);
}

static void test_hardware_lists(void) {
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

static void test_actual_cpu(void) {
    cpu_data_t cpus[MAX_CPU_PACKAGES];
    uint16_t cpu_map[MAX_CPU_PACKAGES];
    uint8_t packages = 0;

    memset(cpus, 0, sizeof(cpus));
    memset(cpu_map, 0xFF, sizeof(cpu_map));

    hw_parse_cpu_topology(cpus, cpu_map, &packages);
    TEST_CHECK(packages > 0);
    TEST_CHECK(packages <= MAX_CPU_PACKAGES);

    hw_get_cpu_model(cpus, cpu_map);
    TEST_CHECK(strlen(cpus[0].model) > 0);
    TEST_CHECK(cpus[0].frequency >= 0.0);

    test_capture_t capture;
    TEST_CHECK(test_capture_begin(&capture));
    hw_get_cpu_info();
    char *output = test_capture_end(&capture);
    TEST_CHECK(output != NULL);
    TEST_STR_CONTAINS(output, "CPU: ");
    free(output);
}

static void test_actual_hardware(void) {
    test_capture_t capture;
    char *output;

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

int main(void) {
    test_sanitize_cpu_name();
    test_gpu_lookup();
    test_battery_metrics();
    test_hardware_lists();
    test_actual_cpu();
    test_actual_hardware();

    return test_finish("linux_hardware");
}
