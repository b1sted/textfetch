/* SPDX-License-Identifier: MIT */

#include "sys_utils.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sys/wait.h>
#include <unistd.h>

#include "../support/test_framework.h"
#include "../support/test_helpers.h"
#include "binary_trees.h"
#include "config.h"

static void test_string_in_array(void)
{
    const char *array[] = {"world", "planet", NULL};

    TEST_CHECK(util_string_in_array("hello world", array) != NULL);
    TEST_STR_EQ(util_string_in_array("hello world", array), "world");
    TEST_CHECK(util_string_in_array("hello", array) == NULL);
    TEST_CHECK(util_string_in_array(NULL, array) == NULL);
    TEST_CHECK(util_string_in_array("hello", NULL) == NULL);

    const char *empty[] = {NULL};
    TEST_CHECK(util_string_in_array("hello", empty) == NULL);
}

static void test_string_lookup(void)
{
    const char *dict[][2] = {
        {"alpha", "A"},
        {"beta", "B"},
        {"", "empty"},
    };

    TEST_STR_EQ(util_string_lookup("alpha", dict, 3), "A");
    TEST_STR_EQ(util_string_lookup("beta", dict, 3), "B");
    TEST_STR_EQ(util_string_lookup("", dict, 3), "empty");
    TEST_CHECK(util_string_lookup("ALPHA", dict, 3) == NULL);
    TEST_CHECK(util_string_lookup("gamma", dict, 3) == NULL);
    TEST_CHECK(util_string_lookup(NULL, dict, 3) == NULL);
    TEST_CHECK(util_string_lookup("alpha", NULL, 3) == NULL);
    TEST_CHECK(util_string_lookup("alpha", dict, 0) == NULL);
}

static void test_read_line(void)
{
    char buffer[16];

    char *path = test_write_temp_file("hello\nworld\n");
    TEST_CHECK(path != NULL);
    TEST_CHECK(util_read_line(path, buffer, sizeof(buffer)));
    TEST_STR_EQ(buffer, "hello");
    unlink(path);
    free(path);

    path = test_write_temp_file("no-newline");
    TEST_CHECK(path != NULL);
    TEST_CHECK(util_read_line(path, buffer, sizeof(buffer)));
    TEST_STR_EQ(buffer, "no-newline");
    unlink(path);
    free(path);

    path = test_write_temp_file("");
    TEST_CHECK(path != NULL);
    memset(buffer, 'X', sizeof(buffer));
    TEST_CHECK(!util_read_line(path, buffer, sizeof(buffer)));
    TEST_STR_EQ(buffer, "");
    unlink(path);
    free(path);

    path = test_write_temp_file("0123456789");
    TEST_CHECK(path != NULL);
    TEST_CHECK(util_read_line(path, buffer, 5));
    TEST_STR_EQ(buffer, "0123");
    unlink(path);
    free(path);

    TEST_CHECK(!util_read_line("/tmp/textfetch-does-not-exist", buffer, sizeof(buffer)));
    TEST_CHECK(!util_read_line(NULL, buffer, sizeof(buffer)));
    TEST_CHECK(!util_read_line("/tmp/anything", NULL, sizeof(buffer)));
    TEST_CHECK(!util_read_line("/tmp/anything", buffer, 0));
}

static void test_read_integers(void)
{
    uint8_t u8 = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    int16_t i16 = 0;
    int32_t i32 = 0;

    char *path = test_write_temp_file("255\n");
    TEST_CHECK(util_read_uint8(path, &u8));
    TEST_INT_EQ(u8, 255);
    unlink(path);
    free(path);

    path = test_write_temp_file("256\n");
    TEST_CHECK(!util_read_uint8(path, &u8));
    unlink(path);
    free(path);

    path = test_write_temp_file("-1\n");
    TEST_CHECK(!util_read_uint8(path, &u8));
    unlink(path);
    free(path);

    path = test_write_temp_file("not-a-number\n");
    TEST_CHECK(!util_read_uint8(path, &u8));
    unlink(path);
    free(path);

    path = test_write_temp_file("65535\n");
    TEST_CHECK(util_read_uint16(path, &u16));
    TEST_INT_EQ(u16, 65535);
    unlink(path);
    free(path);

    path = test_write_temp_file("65536\n");
    TEST_CHECK(!util_read_uint16(path, &u16));
    unlink(path);
    free(path);

    path = test_write_temp_file("4294967295\n");
    TEST_CHECK(util_read_uint32(path, &u32));
    TEST_INT_EQ(u32, 4294967295LL);
    unlink(path);
    free(path);

    path = test_write_temp_file("4294967296\n");
    TEST_CHECK(!util_read_uint32(path, &u32));
    unlink(path);
    free(path);

    path = test_write_temp_file("-32768\n");
    TEST_CHECK(util_read_int16(path, &i16));
    TEST_INT_EQ(i16, -32768);
    unlink(path);
    free(path);

    path = test_write_temp_file("32767\n");
    TEST_CHECK(util_read_int16(path, &i16));
    TEST_INT_EQ(i16, 32767);
    unlink(path);
    free(path);

    path = test_write_temp_file("32768\n");
    TEST_CHECK(!util_read_int16(path, &i16));
    unlink(path);
    free(path);

    path = test_write_temp_file("-32769\n");
    TEST_CHECK(!util_read_int16(path, &i16));
    unlink(path);
    free(path);

    path = test_write_temp_file("-2147483648\n");
    TEST_CHECK(util_read_int32(path, &i32));
    TEST_INT_EQ(i32, -2147483648LL);
    unlink(path);
    free(path);

    path = test_write_temp_file("2147483647\n");
    TEST_CHECK(util_read_int32(path, &i32));
    TEST_INT_EQ(i32, 2147483647LL);
    unlink(path);
    free(path);

    path = test_write_temp_file("2147483648\n");
    TEST_CHECK(!util_read_int32(path, &i32));
    unlink(path);
    free(path);

    path = test_write_temp_file("-2147483649\n");
    TEST_CHECK(!util_read_int32(path, &i32));
    unlink(path);
    free(path);

    TEST_CHECK(!util_read_uint8("/tmp/textfetch-does-not-exist", &u8));
    TEST_CHECK(!util_read_uint8(NULL, &u8));
    TEST_CHECK(!util_read_uint8("/tmp/anything", NULL));
    TEST_CHECK(!util_read_uint16("/tmp/anything", NULL));
    TEST_CHECK(!util_read_uint32("/tmp/anything", NULL));
    TEST_CHECK(!util_read_int16("/tmp/anything", NULL));
    TEST_CHECK(!util_read_int32("/tmp/anything", NULL));
}

static void test_read_hex(void)
{
    uint16_t hex16 = 0;
    uint32_t hex32 = 0;

    char *path = test_write_temp_file("1a2b\n");
    TEST_CHECK(util_read_hex(path, &hex32));
    TEST_INT_EQ(hex32, 0x1a2b);
    unlink(path);
    free(path);

    path = test_write_temp_file("0xFFFF\n");
    TEST_CHECK(util_read_hex16(path, &hex16));
    TEST_INT_EQ(hex16, 0xFFFF);
    unlink(path);
    free(path);

    path = test_write_temp_file("10000\n");
    TEST_CHECK(!util_read_hex16(path, &hex16));
    unlink(path);
    free(path);

    path = test_write_temp_file("FFFFFFFF\n");
    TEST_CHECK(util_read_hex(path, &hex32));
    TEST_INT_EQ(hex32, 0xFFFFFFFFLL);
    unlink(path);
    free(path);

    path = test_write_temp_file("100000000\n");
    TEST_CHECK(!util_read_hex(path, &hex32));
    unlink(path);
    free(path);

    path = test_write_temp_file("not-hex\n");
    TEST_CHECK(!util_read_hex(path, &hex32));
    unlink(path);
    free(path);

    path = test_write_temp_file("1a2b trailing\n");
    TEST_CHECK(!util_read_hex(path, &hex32));
    unlink(path);
    free(path);

    path = test_write_temp_file("FFFFFFFFFFFFFFFFFFFF\n");
    TEST_CHECK(!util_read_hex(path, &hex32));
    unlink(path);
    free(path);

    TEST_CHECK(!util_read_hex("/tmp/textfetch-does-not-exist", &hex32));
    TEST_CHECK(!util_read_hex(NULL, &hex32));
    TEST_CHECK(!util_read_hex("/tmp/anything", NULL));
    TEST_CHECK(!util_read_hex16("/tmp/anything", NULL));
}

static void test_file_exists(void)
{
    char *path = test_write_temp_file("content");
    TEST_CHECK(path != NULL);
    TEST_CHECK(util_is_file_exist(path));
    unlink(path);
    free(path);

    TEST_CHECK(!util_is_file_exist("/tmp/textfetch-does-not-exist"));
    TEST_CHECK(!util_is_file_exist(NULL));
}

static void test_numeric_string(void)
{
    TEST_CHECK(util_is_numeric_string("0"));
    TEST_CHECK(util_is_numeric_string("1234567890"));
    TEST_CHECK(!util_is_numeric_string(""));
    TEST_CHECK(!util_is_numeric_string("12a"));
    TEST_CHECK(!util_is_numeric_string("a12"));
    TEST_CHECK(!util_is_numeric_string("-1"));
    TEST_CHECK(!util_is_numeric_string(" 1"));
    TEST_CHECK(!util_is_numeric_string("1 "));
}

static void test_duplicate_hardware(void)
{
    char buffer[256];

    const char *single[] = {"GPU A"};
    memset(buffer, 0, sizeof(buffer));
    util_format_duplicate_hardware(single, 1, buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "GPU A");

    const char *distinct[] = {"GPU A", "GPU B"};
    memset(buffer, 0, sizeof(buffer));
    util_format_duplicate_hardware(distinct, 2, buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "GPU A\n     GPU B");

    const char *duplicates[] = {"GPU A", "GPU A", "GPU B", "GPU B", "GPU B"};
    memset(buffer, 0, sizeof(buffer));
    util_format_duplicate_hardware(duplicates, 5, buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "2 x GPU A\n     3 x GPU B");

    const char *mixed[] = {NULL, "GPU A", "", "GPU A"};
    memset(buffer, 0, sizeof(buffer));
    util_format_duplicate_hardware(mixed, 4, buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "2 x GPU A");

    memset(buffer, 0, sizeof(buffer));
    util_format_duplicate_hardware(duplicates, 0, buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "");

    memset(buffer, 0, sizeof(buffer));
    util_format_duplicate_hardware(duplicates, 5, buffer, 8);
    TEST_CHECK(strlen(buffer) < 8);
    TEST_CHECK(buffer[7] == '\0');

    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "prefix");
    util_format_duplicate_hardware(single, 1, buffer, sizeof(buffer));
    TEST_STR_EQ(buffer, "prefix\n     GPU A");
}

static bool format_size_in_child(bool kib, bool mib, bool gib, double total, double used,
                                 data_unit_t from_unit, char *out_buf, size_t out_size)
{
    int pipe_fds[2] = {-1, -1};
    pid_t pid = -1;
    int status = 0;
    ssize_t read_count = 0;
    bool result = false;

    if (pipe(pipe_fds) != 0) {
        goto cleanup;
    }

    pid = fork();
    if (pid < 0) {
        goto cleanup;
    }

    if (pid == 0) {
        close(pipe_fds[0]);
        pipe_fds[0] = -1;

        if (kib) {
            util_set_output_unit(UNIT_KIB);
        }
        if (mib) {
            util_set_output_unit(UNIT_MIB);
        }
        if (gib) {
            util_set_output_unit(UNIT_GIB);
        }

        char buffer[128] = {0};
        util_format_size(total, used, buffer, sizeof(buffer), from_unit);

        const char *cursor = buffer;
        size_t remaining = strlen(buffer) + 1;
        while (remaining > 0) {
            ssize_t written = write(pipe_fds[1], cursor, remaining);
            if (written <= 0) {
                break;
            }
            cursor += written;
            remaining -= (size_t)written;
        }

        close(pipe_fds[1]);
        _exit(EXIT_SUCCESS);
    }

    close(pipe_fds[1]);
    pipe_fds[1] = -1;

    read_count = read(pipe_fds[0], out_buf, out_size - 1);
    close(pipe_fds[0]);
    pipe_fds[0] = -1;

    waitpid(pid, &status, 0);

    if (read_count <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
        goto cleanup;
    }

    out_buf[read_count] = '\0';
    result = true;

cleanup:
    if (pipe_fds[0] != -1) {
        close(pipe_fds[0]);
    }
    if (pipe_fds[1] != -1) {
        close(pipe_fds[1]);
    }
    return result;
}

static void test_format_size(void)
{
    char buffer[128];

    memset(buffer, 0, sizeof(buffer));
    util_format_size(1024, 512, buffer, sizeof(buffer), UNIT_B);
    TEST_STR_EQ(buffer, "0.50 KiB / 1.00 KiB (50%)");

    memset(buffer, 0, sizeof(buffer));
    util_format_size(0, 0, buffer, sizeof(buffer), UNIT_B);
    TEST_STR_EQ(buffer, "0.00 B / 0.00 B (0%)");

    memset(buffer, 0, sizeof(buffer));
    util_format_size(1.5, 0.75, buffer, sizeof(buffer), UNIT_GIB);
    TEST_STR_EQ(buffer, "0.75 GiB / 1.50 GiB (50%)");

    memset(buffer, 0, sizeof(buffer));
    util_format_size(2048, 1024, buffer, sizeof(buffer), UNIT_KIB);
    TEST_STR_EQ(buffer, "1.00 MiB / 2.00 MiB (50%)");

    memset(buffer, 0, sizeof(buffer));
    util_format_size(1024, 2048, buffer, sizeof(buffer), UNIT_B);
    TEST_STR_EQ(buffer, "2.00 KiB / 1.00 KiB (100%)");

    memset(buffer, 0, sizeof(buffer));
    util_format_size(512, 2048, buffer, sizeof(buffer), UNIT_B);
    TEST_STR_EQ(buffer, "2.00 KiB / 0.50 KiB (100%)");

    memset(buffer, 0, sizeof(buffer));
    util_format_size(32.0 * 1024 * 1024 * 1024, 0, buffer, sizeof(buffer), UNIT_B);
    TEST_STR_EQ(buffer, "0.00 GiB / 32.00 GiB (0%)");

    memset(buffer, 0, sizeof(buffer));
    TEST_CHECK(
        format_size_in_child(true, false, false, 1048576, 524288, UNIT_B, buffer, sizeof(buffer)));
    TEST_STR_EQ(buffer, "512 KiB / 1024 KiB (50%)");

    memset(buffer, 0, sizeof(buffer));
    TEST_CHECK(format_size_in_child(false, true, false, 1073741824, 536870912, UNIT_B, buffer,
                                    sizeof(buffer)));
    TEST_STR_EQ(buffer, "512 MiB / 1024 MiB (50%)");

    memset(buffer, 0, sizeof(buffer));
    TEST_CHECK(format_size_in_child(false, false, true, 2, 1, UNIT_GIB, buffer, sizeof(buffer)));
    TEST_STR_EQ(buffer, "1.00 GiB / 2.00 GiB (50%)");

    memset(buffer, 0, sizeof(buffer));
    TEST_CHECK(format_size_in_child(true, true, false, 2, 1, UNIT_GIB, buffer, sizeof(buffer)));
    TEST_STR_EQ(buffer, "1024 MiB / 2048 MiB (50%)");

    memset(buffer, 0, sizeof(buffer));
    TEST_CHECK(format_size_in_child(true, true, true, 2, 1, UNIT_GIB, buffer, sizeof(buffer)));
    TEST_STR_EQ(buffer, "1.00 GiB / 2.00 GiB (50%)");

    char small[8];
    memset(small, 'X', sizeof(small));
    util_format_size(1024, 512, small, sizeof(small), UNIT_B);
    TEST_CHECK(small[sizeof(small) - 1] == '\0');
}

static tree_forest_t *parse_ids_from_content(const char *content, size_t capacity)
{
    char *path = NULL;
    tree_forest_t *ids = NULL;

    path = test_write_temp_file(content);
    if (!path) {
        goto cleanup;
    }

    ids = util_parse_ids_file(path, capacity);

cleanup:
    if (path) {
        unlink(path);
        free(path);
    }
    return ids;
}

static void test_parse_ids_file(void)
{
    const char *content = "# comment line\n"
                          "\n"
                          "10de  NVIDIA\n"
                          "\t1c82  GeForce GTX 1050 Ti\n"
                          "\t0de1  GeForce GT 630\n"
                          "1002  AMD\n"
                          "\t73ff  Navi 23\n";

    tree_forest_t *ids = parse_ids_from_content(content, 2);
    TEST_CHECK(ids != NULL);
    if (ids) {
        TEST_INT_EQ(ids->count, 2);
        TEST_CHECK(ids->capacity >= 2);

        tree_node_t *nvidia = tree_find_root(ids, 0x10de);
        tree_node_t *amd = tree_find_root(ids, 0x1002);
        TEST_CHECK(nvidia != NULL);
        TEST_CHECK(amd != NULL);
        if (nvidia && amd) {
            TEST_STR_EQ(nvidia->name, "NVIDIA");
            TEST_STR_EQ(amd->name, "AMD");

            const tree_node_t *gtx = tree_find(nvidia, 0x1c82);
            const tree_node_t *gt630 = tree_find(nvidia, 0x0de1);
            const tree_node_t *navi = tree_find(amd, 0x73ff);
            TEST_CHECK(gtx != NULL);
            TEST_CHECK(gt630 != NULL);
            TEST_CHECK(navi != NULL);
            TEST_STR_EQ(gtx->name, "GeForce GTX 1050 Ti");
            TEST_STR_EQ(gt630->name, "GeForce GT 630");
            TEST_STR_EQ(navi->name, "Navi 23");
        }

        TEST_CHECK(tree_find(nvidia, 0xFFFF) == NULL);
        TEST_CHECK(tree_find_root(ids, 0x1c82) == NULL);
        tree_destroy_forest(ids);
    }

    ids = parse_ids_from_content("", 4);
    TEST_CHECK(ids != NULL);
    if (ids) {
        TEST_INT_EQ(ids->count, 0);
        tree_destroy_forest(ids);
    }

    ids = parse_ids_from_content("\t1c82  Device Without Vendor\n", 4);
    TEST_CHECK(ids != NULL);
    if (ids) {
        TEST_INT_EQ(ids->count, 0);
        tree_destroy_forest(ids);
    }

    ids = parse_ids_from_content("10de  NVIDIA\r\n\t1c82  Device\r\n", 1);
    TEST_CHECK(ids != NULL);
    if (ids) {
        TEST_INT_EQ(ids->count, 1);
        TEST_STR_EQ(ids->roots[0]->name, "NVIDIA");
        TEST_CHECK(tree_find(ids->roots[0], 0x1c82) != NULL);
        tree_destroy_forest(ids);
    }

    ids = parse_ids_from_content("10de  NVIDIA\n"
                                 "\t1c82  Device\n"
                                 "\t\tdead  Subsystem Entry\n"
                                 "C 00  Unclassified device\n"
                                 "\t00  Non-VGA unclassified device\n"
                                 "\t\t00  VGA compatible unclassified device\n",
                                 1);
    TEST_CHECK(ids != NULL);
    if (ids) {
        TEST_INT_EQ(ids->count, 1);
        TEST_CHECK(tree_find(ids->roots[0], 0x1c82) != NULL);
        TEST_CHECK(tree_find(ids->roots[0], 0xdead) == NULL);
        TEST_CHECK(tree_find_root(ids, 0x000c) == NULL);
        tree_destroy_forest(ids);
    }

    char long_name[400];
    char long_content[512];
    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    snprintf(long_content, sizeof(long_content), "10de  NVIDIA\n\t1c82  %s\n", long_name);

    ids = parse_ids_from_content(long_content, 1);
    TEST_CHECK(ids != NULL);
    if (ids) {
        const tree_node_t *device = tree_find(ids->roots[0], 0x1c82);
        TEST_CHECK(device != NULL);
        if (device) {
            TEST_CHECK(strlen(device->name) == sizeof(long_name) - 1);
        }
        tree_destroy_forest(ids);
    }

    const char buffer_content[] = "10de  NVIDIA\n\t1c82  Device Without Newline";
    ids = util_parse_ids_buffer(buffer_content, strlen(buffer_content), 1);
    TEST_CHECK(ids != NULL);
    if (ids) {
        const tree_node_t *device = tree_find(ids->roots[0], 0x1c82);
        TEST_CHECK(device != NULL);
        if (device) {
            TEST_STR_EQ(device->name, "Device Without Newline");
        }
        tree_destroy_forest(ids);
    }

    ids = util_parse_ids_buffer(NULL, 0, 1);
    TEST_CHECK(ids != NULL);
    if (ids) {
        TEST_INT_EQ(ids->count, 0);
        tree_destroy_forest(ids);
    }

    const char filtered_content[] = "1002  AMD\n"
                                    "\t73ff  Navi 23\n"
                                    "\t1234  Other AMD GPU\n"
                                    "10de  NVIDIA\n"
                                    "\t1c82  GeForce GTX 1050 Ti\n"
                                    "\t0de1  GeForce GT 630\n"
                                    "8086  Intel\n"
                                    "\t1234  Intel GPU\n";
    const util_ids_query_t queries[] = {{0x10de, 0x1c82}, {0x1002, 0x73ff}};

    ids = util_parse_ids_buffer_filtered(filtered_content, strlen(filtered_content), 2, queries, 2);
    TEST_CHECK(ids != NULL);
    if (ids) {
        TEST_INT_EQ(ids->count, 2);

        tree_node_t *nvidia = tree_find_root(ids, 0x10de);
        tree_node_t *amd = tree_find_root(ids, 0x1002);
        TEST_CHECK(nvidia != NULL);
        TEST_CHECK(amd != NULL);
        if (nvidia && amd) {
            TEST_CHECK(tree_find(nvidia, 0x1c82) != NULL);
            TEST_CHECK(tree_find(nvidia, 0x0de1) == NULL);
            TEST_CHECK(tree_find(amd, 0x73ff) != NULL);
            TEST_CHECK(tree_find(amd, 0x1234) == NULL);
        }
        TEST_CHECK(tree_find_root(ids, 0x8086) == NULL);
        tree_destroy_forest(ids);
    }

    const util_ids_query_t missing_device[] = {{0x10de, 0xffff}};
    ids = util_parse_ids_buffer_filtered(filtered_content, strlen(filtered_content), 1,
                                         missing_device, 1);
    TEST_CHECK(ids != NULL);
    if (ids) {
        TEST_INT_EQ(ids->count, 1);
        TEST_CHECK(tree_find(ids->roots[0], 0xffff) == NULL);
        tree_destroy_forest(ids);
    }

    const util_ids_query_t missing_vendor[] = {{0x9999, 0x0001}};
    ids = util_parse_ids_buffer_filtered(filtered_content, strlen(filtered_content), 1,
                                         missing_vendor, 1);
    TEST_CHECK(ids != NULL);
    if (ids) {
        TEST_INT_EQ(ids->count, 0);
        tree_destroy_forest(ids);
    }

    char *filtered_path = test_write_temp_file(filtered_content);
    TEST_CHECK(filtered_path != NULL);
    if (filtered_path) {
        ids = util_parse_ids_file_filtered(filtered_path, 2, queries, 2);
        TEST_CHECK(ids != NULL);
        if (ids) {
            TEST_INT_EQ(ids->count, 2);
            TEST_CHECK(tree_find_root(ids, 0x10de) != NULL);
            TEST_CHECK(tree_find_root(ids, 0x1002) != NULL);
            tree_destroy_forest(ids);
        }
        unlink(filtered_path);
        free(filtered_path);
    }

    TEST_CHECK(util_parse_ids_file(NULL, 4) == NULL);
    TEST_CHECK(util_parse_ids_buffer(NULL, 4, 4) == NULL);
    TEST_CHECK(util_parse_ids_file_filtered(NULL, 4, queries, 2) == NULL);
    TEST_CHECK(util_parse_ids_file_filtered("missing", 4, NULL, 2) == NULL);
    TEST_CHECK(util_parse_ids_file_filtered("missing", 4, queries, 0) == NULL);
    TEST_CHECK(util_parse_ids_buffer_filtered(filtered_content, strlen(filtered_content), 4, NULL,
                                              1) == NULL);
    TEST_CHECK(util_parse_ids_buffer_filtered(filtered_content, strlen(filtered_content), 4,
                                              queries, 0) == NULL);
    TEST_CHECK(parse_ids_from_content("10de  NVIDIA\n", 0) == NULL);
}

int main(void)
{
    test_string_in_array();
    test_string_lookup();
    test_read_line();
    test_read_integers();
    test_read_hex();
    test_file_exists();
    test_numeric_string();
    test_duplicate_hardware();
    test_format_size();
    test_parse_ids_file();

    return test_finish("sys_utils");
}
