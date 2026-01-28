/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"

/**
 * FNV-1a Hash Algorithm (64-bit variant).
 * 
 * @param key The null-terminated string to hash.
 * @return The calculated 64-bit hash value.
 */
static uint64_t hash_key(const char* key);

/**
 * Calculates the next power of two for a given number.
 * Ensures the capacity is compatible with bitwise masking.
 * 
 * @param n The minimum required capacity.
 * @return The smallest power of two greater than or equal to n.
 */
static size_t next_pow2(size_t n);

/**
 * Expands the hash table to a new capacity and rehashes all existing keys.
 * 
 * @param set Pointer to the string set to resize.
 * @param new_capacity The target capacity (must be a power of two).
 * @return true if successful, false if memory allocation failed.
 */
static bool strset_resize(string_set_t* set, size_t new_capacity);

string_set_t* strset_create(size_t capacity) {
    string_set_t* set = malloc(sizeof(string_set_t));
    if (!set) return NULL;

    set->capacity = next_pow2(capacity);
    set->count = 0;
    set->entries = calloc(set->capacity, sizeof(entry_t));
    
    if (!set->entries) {
        free(set);
        return NULL;
    }

    return set;
}

bool strset_add(string_set_t* set, const char* key) {
    if (!set || !key) return false;

    if ((set->count + 1) * 4 > set->capacity * 3) {
        if (!strset_resize(set, set->capacity * 2)) return false;
    }

    uint64_t hash = hash_key(key);
    size_t index = hash & (set->capacity - 1);

    while (set->entries[index].key != NULL) {
        if (strcmp(set->entries[index].key, key) == 0) {
            return false;
        }

        index = (index + 1) & (set->capacity - 1);
    }

    char* copy = strdup(key);
    if (!copy) return false;

    set->entries[index].key = copy;
    set->count++;
    return true;
}

bool strset_contains(string_set_t* set, const char* key) {
    if (!set || !key) return false;

    uint64_t hash = hash_key(key);
    size_t index = hash & (set->capacity - 1);

    while (set->entries[index].key != NULL) {
        if (strcmp(set->entries[index].key, key) == 0) {
            return true;
        }

        index = (index + 1) & (set->capacity - 1);
    }

    return false;
}

void strset_destroy(string_set_t* set) {
    if (!set) return;
    
    for (size_t i = 0; i < set->capacity; i++) {
        if (set->entries[i].key) {
            free(set->entries[i].key);
        }
    }

    free(set->entries);
    free(set);
}

static uint64_t hash_key(const char* key) {
    uint64_t hash = 0xcbf29ce484222325; // FNV_offset_basis
    while (*key) {
        hash ^= (unsigned char)*key++;
        hash *= 0x100000001b3;          // FNV_prime
    }
    return hash;
}

static size_t next_pow2(size_t n) {
    size_t res = 1;
    while (res < n) res <<= 1;
    return res;
}

static bool strset_resize(string_set_t* set, size_t new_capacity) {
    entry_t* new_entries = calloc(new_capacity, sizeof(entry_t));
    if (!new_entries) return false;

    for (size_t i = 0; i < set->capacity; i++) {
        if (set->entries[i].key == NULL) continue;

        uint64_t hash = hash_key(set->entries[i].key);
        size_t index = hash & (new_capacity - 1);

        while (new_entries[index].key != NULL) {
            index = (index + 1) & (new_capacity - 1);
        }
        new_entries[index].key = set->entries[i].key;
    }

    free(set->entries);
    set->entries = new_entries;
    set->capacity = new_capacity;
    return true;
}
