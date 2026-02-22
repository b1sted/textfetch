/* SPDX-License-Identifier: MIT */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "bitset.h"
#include "sys_utils.h"

void set_add(bitset_t *set, size_t num) {
    if (num < set->capacity) {
        set->bits[num / 32] |= (1U << (num % 32));
        return;
    }
    
    V_PRINTF("[ERROR] Bitset add overflow: value %zu exceeds uint32_t limit\n", num);
}

bool set_contains(const bitset_t *set, size_t num) {
    if (num < set->capacity) {
        return (set->bits[num / 32] & (1U << (num % 32))) != 0;
    }

    V_PRINTF("[ERROR] Bitset check overflow: value %zu exceeds uint32_t limit\n", num);
    return false;
}

void set_remove(bitset_t *set, size_t num) {
    if (num < set->capacity) {
        set->bits[num / 32] &= ~(1U << (num % 32));
        return;
    }

    V_PRINTF("[ERROR] Bitset remove overflow: value %zu exceeds uint32_t limit\n", num);
}

uint32_t count_set_bits(bitset_t *set, size_t blocks) {
    uint32_t total = 0;

    for (size_t i = 0; i < blocks; i++) {
        total += __builtin_popcount(set->bits[i]);
    }

    return total;
}
