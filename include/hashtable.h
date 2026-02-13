/* SPDX-License-Identifier: MIT */

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Default starting capacity.
 * Must be a power of two for efficient bitwise indexing.
 */
#define INITIAL_CAPACITY 256

/**
 * Hash table entry.
 * Represents a single bucket in the set.
 */
typedef struct {
    char *key; /* Pointer to the string key. NULL if slot is empty. */
} entry_t;

/**
 * String Set structure.
 * Implements a hash set using open addressing and linear probing.
 */
typedef struct {
    entry_t *entries; /* Dynamic array of buckets. */
    size_t capacity;  /* Total allocated slots (always a power of 2). */
    size_t count;     /* Number of active elements in the set. */
} string_set_t;

/**
 * Allocates and initializes a new string set.
 *
 * @param capacity Requested initial size (will be rounded up to power of 2).
 * @return Pointer to set or NULL on allocation failure.
 */
string_set_t *strset_create(size_t capacity);

/**
 * Inserts a unique string into the set.
 * Internally duplicates the key and handles table expansion.
 *
 * @param set Pointer to the set.
 * @param key String to insert.
 * @return true if inserted, false if already exists or memory error.
 */
bool strset_add(string_set_t *set, const char *key);

/**
 * Performs a lookup for a specific key.
 *
 * @param set Pointer to the set.
 * @param key String to find.
 * @return true if key exists, false otherwise.
 */
bool strset_contains(string_set_t *set, const char *key);

/**
 * Deep-cleans the set and all its stored strings.
 *
 * @param set Pointer to the set to destroy.
 */
void strset_destroy(string_set_t *set);

#endif /* HASHTABLE_H */