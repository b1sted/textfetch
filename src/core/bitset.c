/* SPDX-License-Identifier: MIT */

#include "bitset.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sys_utils.h"

void set_add(bitset_t *set, size_t num)
{
    if (!set) {
        return;
    }

    if (num < set->capacity) {
        set->bits[num / 32] |= (1U << (num % 32));
        return;
    }

    V_PRINTF("[ERROR] Bitset add overflow: value %zu exceeds uint32_t limit\n", num);
}

bool set_contains(const bitset_t *set, size_t num)
{
    if (!set) {
        return false;
    }

    if (num < set->capacity) {
        return (set->bits[num / 32] & (1U << (num % 32))) != 0;
    }

    V_PRINTF("[ERROR] Bitset check overflow: value %zu exceeds uint32_t limit\n", num);
    return false;
}

void set_remove(bitset_t *set, size_t num)
{
    if (!set) {
        return;
    }

    if (num < set->capacity) {
        set->bits[num / 32] &= ~(1U << (num % 32));
        return;
    }

    V_PRINTF("[ERROR] Bitset remove overflow: value %zu exceeds uint32_t limit\n", num);
}

static uint32_t set_popcount32(uint32_t value)
{
    uint32_t count = 0;

    while (value != 0) {
        value &= value - 1U;
        count++;
    }

    return count;
}

uint32_t set_count_bits(const bitset_t *set, size_t blocks)
{
    if (!set || !set->bits) {
        return 0;
    }

    size_t max_blocks = set->capacity / BITS_PER_BLOCK;
    if (set->capacity % BITS_PER_BLOCK != 0) {
        max_blocks++;
    }

    if (blocks > max_blocks) {
        blocks = max_blocks;
    }

    uint32_t total = 0;

    for (size_t i = 0; i < blocks; i++) {
        total += set_popcount32(set->bits[i]);
    }

    return total;
}
