/* SPDX-License-Identifier: MIT */

#if defined(__APPLE__) && defined(__MACH__)

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPSKeys.h>

#include "../support/test_framework.h"

/* Include the translation unit to exercise its static helpers directly. */
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../../src/platform/macos/hardware.c"

static void test_cf_value_to_string_guards(void)
{
    char buffer[32] = {0};

    TEST_CHECK(!cf_value_to_string(NULL, buffer, sizeof(buffer)));
    TEST_CHECK(!cf_value_to_string(NULL, NULL, 0));
    TEST_CHECK(!cf_value_to_string((CFTypeRef)CFSTR("value"), NULL, 0));
    TEST_CHECK(!cf_value_to_string((CFTypeRef)CFSTR("value"), buffer, 0));

    memset(buffer, 'X', sizeof(buffer));
    TEST_CHECK(!cf_value_to_string((CFTypeRef)CFSTR("value"), buffer, 0));
    TEST_INT_EQ(buffer[0], 'X');

    CFStringRef string_value = CFSTR("Apple M4");
    TEST_CHECK(cf_value_to_string((CFTypeRef)string_value, buffer, sizeof(buffer)));
    TEST_STR_EQ(buffer, "Apple M4");

    memset(buffer, 'X', sizeof(buffer));
    CFNumberRef number_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &(int){42});
    TEST_CHECK(number_value != NULL);
    if (number_value) {
        TEST_CHECK(!cf_value_to_string((CFTypeRef)number_value, buffer, sizeof(buffer)));
        TEST_STR_EQ(buffer, "");
        CFRelease(number_value);
    }

    const uint8_t data_bytes[] = {'O', 'K', '\0', 'X'};
    CFDataRef data_value =
        CFDataCreate(kCFAllocatorDefault, data_bytes, (CFIndex)sizeof(data_bytes));
    TEST_CHECK(data_value != NULL);
    if (data_value) {
        TEST_CHECK(cf_value_to_string((CFTypeRef)data_value, buffer, sizeof(buffer)));
        TEST_STR_EQ(buffer, "OK");
        CFRelease(data_value);
    }

    CFDataRef empty_data = CFDataCreate(kCFAllocatorDefault, NULL, 0);
    TEST_CHECK(empty_data != NULL);
    if (empty_data) {
        TEST_CHECK(!cf_value_to_string((CFTypeRef)empty_data, buffer, sizeof(buffer)));
        TEST_STR_EQ(buffer, "");
        CFRelease(empty_data);
    }
}

static CFDictionaryRef make_battery_dictionary(int current_capacity, int max_capacity)
{
    CFMutableDictionaryRef dictionary = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!dictionary) {
        return NULL;
    }

    CFNumberRef current = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &current_capacity);
    CFNumberRef maximum = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &max_capacity);

    if (!current || !maximum) {
        if (current) {
            CFRelease(current);
        }
        if (maximum) {
            CFRelease(maximum);
        }
        CFRelease(dictionary);
        return NULL;
    }

    CFDictionarySetValue(dictionary, CFSTR(kIOPSCurrentCapacityKey), current);
    CFDictionarySetValue(dictionary, CFSTR(kIOPSMaxCapacityKey), maximum);

    CFRelease(current);
    CFRelease(maximum);

    return dictionary;
}

static void check_battery_percentage(int current_capacity, int max_capacity, uint8_t expected)
{
    CFDictionaryRef dictionary = make_battery_dictionary(current_capacity, max_capacity);
    TEST_CHECK(dictionary != NULL);
    if (!dictionary) {
        return;
    }

    TEST_INT_EQ(hw_get_bat_percentage(dictionary), expected);
    CFRelease(dictionary);
}

static void test_battery_percentage_clamping(void)
{
    check_battery_percentage(50, 100, 50);
    check_battery_percentage(0, 100, 0);
    check_battery_percentage(100, 100, 100);
    check_battery_percentage(150, 100, 100);
    check_battery_percentage(200, 100, 100);
    check_battery_percentage(-25, 100, 0);
    check_battery_percentage(50, 0, 0);
    check_battery_percentage(50, -100, 0);

    TEST_INT_EQ(hw_get_bat_percentage(NULL), 0);
    TEST_INT_EQ(hw_get_bat_percentage((CFDictionaryRef)CFSTR("not a dictionary")), 0);

    CFMutableDictionaryRef empty_dictionary = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    TEST_CHECK(empty_dictionary != NULL);
    if (empty_dictionary) {
        TEST_INT_EQ(hw_get_bat_percentage(empty_dictionary), 0);
        CFRelease(empty_dictionary);
    }
}

int main(void)
{
    test_cf_value_to_string_guards();
    test_battery_percentage_clamping();

    return test_finish("macos_hardware");
}

#else

#include <stdio.h>

int main(void)
{
    printf("[SKIP] macOS hardware tests skipped on non-Apple host\n");
    return 0;
}

#endif
