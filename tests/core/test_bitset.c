/* SPDX-License-Identifier: MIT */

#include <stdint.h>

#include "../support/test_framework.h"
#include "bitset.h"

static void test_size_macros(void)
{
    TEST_INT_EQ(SET_SIZE(0), 0);
    TEST_INT_EQ(SET_SIZE(1), 1);
    TEST_INT_EQ(SET_SIZE(31), 1);
    TEST_INT_EQ(SET_SIZE(32), 1);
    TEST_INT_EQ(SET_SIZE(33), 2);
    TEST_INT_EQ(SET_SIZE(64), 2);
    TEST_INT_EQ(SET_SIZE(65), 3);

    TEST_INT_EQ(BIT(0), 1U);
    TEST_INT_EQ(BIT(1), 2U);
    TEST_INT_EQ(BIT(31), 0x80000000U);
    TEST_INT_EQ(BIT(32), 1U);
    TEST_INT_EQ(BIT(33), 2U);
}

static void test_add_contains_remove(void)
{
    uint32_t blocks[SET_SIZE(128)] = {0};
    bitset_t set = {.bits = blocks, .capacity = 128};

    TEST_CHECK(!set_contains(&set, 0));
    TEST_CHECK(!set_contains(&set, 31));
    TEST_CHECK(!set_contains(&set, 32));
    TEST_CHECK(!set_contains(&set, 127));

    set_add(&set, 0);
    set_add(&set, 31);
    set_add(&set, 32);
    set_add(&set, 127);

    TEST_CHECK(set_contains(&set, 0));
    TEST_CHECK(set_contains(&set, 31));
    TEST_CHECK(set_contains(&set, 32));
    TEST_CHECK(set_contains(&set, 127));
    TEST_CHECK(!set_contains(&set, 1));
    TEST_CHECK(!set_contains(&set, 126));
    TEST_INT_EQ(count_set_bits(&set, SET_SIZE(128)), 4);

    set_remove(&set, 31);
    TEST_CHECK(!set_contains(&set, 31));
    TEST_CHECK(set_contains(&set, 32));
    TEST_INT_EQ(count_set_bits(&set, SET_SIZE(128)), 3);

    set_remove(&set, 31);
    set_remove(&set, 100);
    TEST_INT_EQ(count_set_bits(&set, SET_SIZE(128)), 3);

    set_add(&set, 31);
    set_add(&set, 31);
    TEST_CHECK(set_contains(&set, 31));
    TEST_INT_EQ(count_set_bits(&set, SET_SIZE(128)), 4);
}

static void test_out_of_range(void)
{
    uint32_t blocks[SET_SIZE(64)] = {0};
    bitset_t set = {.bits = blocks, .capacity = 64};

    set_add(&set, 64);
    set_add(&set, 1000);
    TEST_CHECK(!set_contains(&set, 64));
    TEST_CHECK(!set_contains(&set, 1000));
    TEST_INT_EQ(count_set_bits(&set, SET_SIZE(64)), 0);

    set_add(&set, 63);
    TEST_CHECK(set_contains(&set, 63));
    set_remove(&set, 64);
    TEST_CHECK(set_contains(&set, 63));
    TEST_INT_EQ(count_set_bits(&set, SET_SIZE(64)), 1);
}

static void test_zero_capacity(void)
{
    bitset_t set = {.bits = NULL, .capacity = 0};

    set_add(&set, 0);
    TEST_CHECK(!set_contains(&set, 0));
    set_remove(&set, 0);
    TEST_INT_EQ(count_set_bits(&set, 0), 0);
}

static void test_null_set(void)
{
    set_add(NULL, 0);
    set_remove(NULL, 0);
    TEST_CHECK(!set_contains(NULL, 0));
    TEST_INT_EQ(count_set_bits(NULL, 4), 0);
}

static void test_count_blocks(void)
{
    uint32_t blocks[SET_SIZE(128)] = {0};
    bitset_t set = {.bits = blocks, .capacity = 128};

    for (size_t i = 0; i < 128; i += 2) {
        set_add(&set, i);
    }

    TEST_INT_EQ(count_set_bits(&set, 0), 0);
    TEST_INT_EQ(count_set_bits(&set, 1), 16);
    TEST_INT_EQ(count_set_bits(&set, 2), 32);
    TEST_INT_EQ(count_set_bits(&set, SET_SIZE(128)), 64);
}

int main(void)
{
    test_size_macros();
    test_add_contains_remove();
    test_out_of_range();
    test_zero_capacity();
    test_null_set();
    test_count_blocks();

    return test_finish("bitset");
}
