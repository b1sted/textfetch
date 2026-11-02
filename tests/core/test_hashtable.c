/* SPDX-License-Identifier: MIT */

#include "hashtable.h"

#include <stdio.h>

#include "../support/test_framework.h"

static void test_create(void)
{
    string_set_t *set = strset_create(0);
    TEST_CHECK(set != NULL);
    TEST_INT_EQ(set->capacity, 1);
    TEST_INT_EQ(set->count, 0);
    strset_destroy(set);

    set = strset_create(1);
    TEST_CHECK(set != NULL);
    TEST_INT_EQ(set->capacity, 1);
    strset_destroy(set);

    set = strset_create(3);
    TEST_CHECK(set != NULL);
    TEST_INT_EQ(set->capacity, 4);
    strset_destroy(set);

    set = strset_create(INITIAL_CAPACITY);
    TEST_CHECK(set != NULL);
    TEST_INT_EQ(set->capacity, INITIAL_CAPACITY);
    strset_destroy(set);
}

static void test_add_contains(void)
{
    string_set_t *set = strset_create(4);
    TEST_CHECK(set != NULL);

    TEST_CHECK(!strset_contains(set, "alpha"));
    TEST_INT_EQ(strset_get_count(set), 0);

    TEST_CHECK(strset_add(set, "alpha"));
    TEST_CHECK(strset_contains(set, "alpha"));
    TEST_INT_EQ(strset_get_count(set), 1);

    TEST_CHECK(!strset_add(set, "alpha"));
    TEST_INT_EQ(strset_get_count(set), 1);

    TEST_CHECK(strset_add(set, "beta"));
    TEST_CHECK(strset_add(set, "gamma"));
    TEST_CHECK(strset_contains(set, "alpha"));
    TEST_CHECK(strset_contains(set, "beta"));
    TEST_CHECK(strset_contains(set, "gamma"));
    TEST_CHECK(!strset_contains(set, "delta"));
    TEST_INT_EQ(strset_get_count(set), 3);

    strset_destroy(set);
}

static void test_resize(void)
{
    string_set_t *set = strset_create(4);
    TEST_CHECK(set != NULL);

    char key[32];
    for (int i = 0; i < 1000; i++) {
        snprintf(key, sizeof(key), "key-%d", i);
        TEST_CHECK(strset_add(set, key));
    }

    TEST_INT_EQ(strset_get_count(set), 1000);
    TEST_CHECK(set->capacity >= 1024);

    for (int i = 0; i < 1000; i++) {
        snprintf(key, sizeof(key), "key-%d", i);
        TEST_CHECK(strset_contains(set, key));
    }

    TEST_CHECK(!strset_contains(set, "key-1000"));
    TEST_CHECK(!strset_add(set, "key-500"));
    TEST_INT_EQ(strset_get_count(set), 1000);

    strset_destroy(set);
}

static void test_empty_and_long_keys(void)
{
    string_set_t *set = strset_create(2);
    TEST_CHECK(set != NULL);

    TEST_CHECK(strset_add(set, ""));
    TEST_CHECK(strset_contains(set, ""));

    char long_key[512];
    for (size_t i = 0; i < sizeof(long_key) - 1; i++) {
        long_key[i] = (char)('a' + (i % 26));
    }
    long_key[sizeof(long_key) - 1] = '\0';

    TEST_CHECK(strset_add(set, long_key));
    TEST_CHECK(strset_contains(set, long_key));
    TEST_INT_EQ(strset_get_count(set), 2);

    strset_destroy(set);
}

static void test_invalid_arguments(void)
{
    string_set_t *set = strset_create(4);
    TEST_CHECK(set != NULL);

    TEST_CHECK(!strset_add(NULL, "alpha"));
    TEST_CHECK(!strset_add(set, NULL));
    TEST_CHECK(!strset_contains(NULL, "alpha"));
    TEST_CHECK(!strset_contains(set, NULL));
    TEST_INT_EQ(strset_get_count(NULL), 0);

    strset_destroy(set);
    strset_destroy(NULL);
}

int main(void)
{
    test_create();
    test_add_contains();
    test_resize();
    test_empty_and_long_keys();
    test_invalid_arguments();

    return test_finish("hashtable");
}
