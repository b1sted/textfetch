/* SPDX-License-Identifier: MIT */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Shared counters for the current test executable. */
static int test_checks = 0;
static int test_failures = 0;

#define TEST_CHECK(condition)                                                 \
    do {                                                                      \
        test_checks++;                                                        \
        if (!(condition)) {                                                   \
            test_failures++;                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,           \
                    #condition);                                              \
        }                                                                     \
    } while (0)

#define TEST_CHECK_MSG(condition, ...)                                        \
    do {                                                                      \
        test_checks++;                                                        \
        if (!(condition)) {                                                   \
            test_failures++;                                                  \
            fprintf(stderr, "FAIL %s:%d: %s: ", __FILE__, __LINE__,           \
                    #condition);                                              \
            fprintf(stderr, __VA_ARGS__);                                     \
            fprintf(stderr, "\n");                                            \
        }                                                                     \
    } while (0)

#define TEST_INT_EQ(actual, expected)                                         \
    do {                                                                      \
        long long test_actual_value = (long long)(actual);                    \
        long long test_expected_value = (long long)(expected);                \
        test_checks++;                                                        \
        if (test_actual_value != test_expected_value) {                       \
            test_failures++;                                                  \
            fprintf(stderr,                                                   \
                    "FAIL %s:%d: %s == %s (actual: %lld, expected: %lld)\n",  \
                    __FILE__, __LINE__, #actual, #expected,                   \
                    test_actual_value, test_expected_value);                  \
        }                                                                     \
    } while (0)

#define TEST_STR_EQ(actual, expected)                                         \
    do {                                                                      \
        const char *test_actual_value = (actual);                             \
        const char *test_expected_value = (expected);                         \
        test_checks++;                                                        \
        if (test_actual_value == NULL || test_expected_value == NULL) {       \
            if (test_actual_value != test_expected_value) {                   \
                test_failures++;                                              \
                fprintf(stderr,                                               \
                        "FAIL %s:%d: %s == %s (actual: %s, expected: %s)\n",  \
                        __FILE__, __LINE__, #actual, #expected,               \
                        test_actual_value ? test_actual_value : "(null)",     \
                        test_expected_value ? test_expected_value : "(null)"); \
            }                                                                 \
        } else if (strcmp(test_actual_value, test_expected_value) != 0) {     \
            test_failures++;                                                  \
            fprintf(stderr,                                                   \
                    "FAIL %s:%d: %s == %s (actual: \"%s\", expected: "       \
                    "\"%s\")\n",                                              \
                    __FILE__, __LINE__, #actual, #expected,                   \
                    test_actual_value, test_expected_value);                  \
        }                                                                     \
    } while (0)

#define TEST_STR_CONTAINS(haystack, needle)                                   \
    do {                                                                      \
        const char *test_haystack = (haystack);                               \
        const char *test_needle = (needle);                                   \
        test_checks++;                                                        \
        if (test_haystack == NULL || test_needle == NULL ||                   \
            strstr(test_haystack, test_needle) == NULL) {                     \
            test_failures++;                                                  \
            fprintf(stderr,                                                   \
                    "FAIL %s:%d: \"%s\" contains \"%s\"\n", __FILE__,         \
                    __LINE__, test_haystack ? test_haystack : "(null)",       \
                    test_needle ? test_needle : "(null)");                    \
        }                                                                     \
    } while (0)

#define TEST_STR_NOT_CONTAINS(haystack, needle)                               \
    do {                                                                      \
        const char *test_haystack = (haystack);                               \
        const char *test_needle = (needle);                                   \
        test_checks++;                                                        \
        if (test_haystack != NULL && test_needle != NULL &&                   \
            strstr(test_haystack, test_needle) != NULL) {                     \
            test_failures++;                                                  \
            fprintf(stderr, "FAIL %s:%d: \"%s\" unexpectedly contains "       \
                    "\"%s\"\n", __FILE__, __LINE__, test_haystack,            \
                    test_needle);                                             \
        }                                                                     \
    } while (0)

static inline int test_finish(const char *suite_name) {
    if (test_failures == 0) {
        printf("PASS %s (%d checks)\n", suite_name, test_checks);
        return EXIT_SUCCESS;
    }

    printf("FAIL %s (%d/%d checks failed)\n", suite_name, test_failures,
           test_checks);
    return EXIT_FAILURE;
}

#endif /* TEST_FRAMEWORK_H */
