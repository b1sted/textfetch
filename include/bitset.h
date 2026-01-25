#ifndef BITSET_H
#define BITSET_H

/**
 * Maximum number of unique elements (IDs) the bitset can handle.
 * Determines the valid range of input integers: [0, MAX_ELEMENTS - 1].
 */
#define MAX_ELEMENTS 256

/**
 * Size of the underlying array buffer in 32-bit blocks.
 * Calculated to ensure enough storage for MAX_ELEMENTS bits.
 * Division by 32 corresponds to sizeof(uint32_t) in bits.
 */
#define SET_SIZE (MAX_ELEMENTS / 32 + 1)

/**
 * Adds an element to the bitset.
 * Sets the bit corresponding to the given number to 1.
 * Performs a boundary check against MAX_ELEMENTS before modification.
 *
 * @param set Pointer to the bitset array.
 * @param num The non-negative integer element to add.
 */
void set_add(uint32_t *set, int num);

/**
 * Checks if an element exists in the bitset.
 * Determines if the bit corresponding to the given number is set to 1.
 *
 * @param set Pointer to the bitset array (read-only).
 * @param num The non-negative integer element to check.
 * @return true if the element is present, false otherwise or if out of bounds.
 */
bool set_contains(const uint32_t *set, int num);

/**
 * Removes an element from the bitset.
 * Clears the bit corresponding to the given number (sets it to 0).
 * Performs a boundary check against MAX_ELEMENTS before modification.
 *
 * @param set Pointer to the bitset array.
 * @param num The non-negative integer element to remove.
 */
void set_remove(uint32_t *set, int num);

/**
 * Calculates the total number of elements in the bitset.
 * Iterates through the array blocks and sums up the population count 
 * (number of set bits) using a compiler builtin for efficiency.
 *
 * @param set Pointer to the bitset array.
 * @param blocks Number of uint32_t blocks in the array (e.g., SET_SIZE).
 * @return The total count of bits set to 1.
 */
uint32_t count_set_bits(uint32_t *set, size_t blocks);

#endif // BITSET_H