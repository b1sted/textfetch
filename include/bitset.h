/* SPDX-License-Identifier: MIT */

#ifndef BITSET_H
#define BITSET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Bitset array block width (typically 32 for uint32_t). */
#define BITS_PER_BLOCK         32

/* Computes block array size, rounding up for remainders. */
#define SET_SIZE(MAX_ELEMENTS) ((MAX_ELEMENTS) + (BITS_PER_BLOCK - 1)) / BITS_PER_BLOCK

/* Generates a bitmask with a single bit set. */
#define BIT(n)                 (1U << ((n) % 32))

/* Array-backed bitset used to track specific IDs or flag presence. */
typedef struct {
    uint32_t *bits;
    size_t capacity;
} bitset_t;

/**
 * Adds an element to the bitset.
 * Sets the bit corresponding to the given number to 1.
 * Performs a boundary check against MAX_ELEMENTS before modification.
 *
 * @param set Pointer to the bitset array.
 * @param num The non-negative integer element to add.
 */
void set_add(bitset_t *set, size_t num);

/**
 * Checks if an element exists in the bitset.
 * Determines if the bit corresponding to the given number is set to 1.
 *
 * @param set Pointer to the bitset array (read-only).
 * @param num The non-negative integer element to check.
 * @return true if the element is present, false otherwise or if out of bounds.
 */
bool set_contains(const bitset_t *set, size_t num);

/**
 * Removes an element from the bitset.
 * Clears the bit corresponding to the given number (sets it to 0).
 * Performs a boundary check against MAX_ELEMENTS before modification.
 *
 * @param set Pointer to the bitset array.
 * @param num The non-negative integer element to remove.
 */
void set_remove(bitset_t *set, size_t num);

/**
 * Calculates the total number of elements in the bitset.
 * Iterates through the array blocks and sums up the population count
 * (number of set bits) using a compiler builtin for efficiency.
 *
 * @param set Pointer to the bitset array.
 * @param blocks Number of uint32_t blocks in the array (e.g., SET_SIZE).
 * @return The total count of bits set to 1.
 */
uint32_t count_set_bits(bitset_t *set, size_t blocks);

#endif /* BITSET_H */