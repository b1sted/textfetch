/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "bitset.h"

void set_add(uint32_t *set, int num) {
    if (num >= 0 && num < MAX_ELEMENTS) {
        set[num / 32] |= (1U << (num % 32));
    }
}

bool set_contains(const uint32_t *set, int num) {
    if (num >= 0 && num < MAX_ELEMENTS) {
        return (set[num / 32] & (1U << (num % 32))) != 0;
    }
    
    return false;
}

void set_remove(uint32_t *set, int num) {
    if (num >= 0 && num < MAX_ELEMENTS) {
        set[num / 32] &= ~(1U << (num % 32));
    }
}

uint32_t count_set_bits(uint32_t *set, size_t blocks) {
    uint32_t total = 0;

    for (size_t i = 0; i < blocks; i++) {
        total += __builtin_popcount(set[i]);
    }

    return total;
}
