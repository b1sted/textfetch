/* SPDX-License-Identifier: MIT */

#ifndef BINARY_TREES_H
#define BINARY_TREES_H

#include <stddef.h>
#include <stdint.h>

/**
 * Node structure for the hierarchical n-ary tree format implementation.
 *
 * @param id Numeric identifier of the node.
 * @param name Human-readable name string.
 */
typedef struct tree_node_t {
    uint16_t id;
    char *name;
    struct tree_node_t *first_child;
    struct tree_node_t *next_sibling;
} tree_node_t;

/* Manager structure grouping multiple root nodes for efficient lookup. */
typedef struct {
    tree_node_t **roots;
    size_t count;
    size_t capacity;
} tree_forest_t;

/**
 * Creates a new binary tree node with dynamic name allocation.
 *
 * @param id The unique identifier for the node.
 * @param name The human-readable name. Will be duplicated using strdup().
 * @return A pointer to the new node, or NULL if memory allocation fails.
 */
tree_node_t *tree_create_node(uint16_t id, const char *name);

/**
 * Allocates and initializes a new forest structure.
 *
 * @param initial_capacity The starting size of the forest's roots array.
 * @return A pointer to the new forest, or NULL on failure.
 */
tree_forest_t *tree_create_forest(size_t initial_capacity);

/**
 * Adds a child node to an existing parent node.
 *
 * @param parent Pointer to the parent node.
 * @param id The identifier for the new child node.
 * @param name The name of the new child node.
 */
void tree_add_child(tree_node_t *parent, uint16_t id, const char *name);

/**
 * Adds an existing tree (represented by its root) to the forest.
 *
 * @param f Pointer to the forest.
 * @param root Pointer to the root node of the tree to add.
 */
void tree_add_to_forest(tree_forest_t *f, tree_node_t *root);

/**
 * Searches for a node with a specific ID within a tree.
 *
 * @param root Pointer to the root of the tree to search.
 * @param target_id The identifier to search for.
 * @return Pointer to the found node, or NULL if not found.
 */
const tree_node_t *tree_find(const tree_node_t *root, uint16_t target_id);

/**
 * Searches for a root node with a specific ID within the forest.
 *
 * @param f Pointer to the forest to search.
 * @param target_id The identifier to search for.
 * @return Pointer to the found root node, or NULL if not found.
 */
tree_node_t *tree_find_root(const tree_forest_t *f, uint16_t target_id);

/**
 * Recursively frees a node and all its children/siblings.
 *
 * @param root Pointer to the node to free.
 */
void tree_free(tree_node_t *root);

/**
 * Destroys a forest, freeing all its trees and internal arrays.
 *
 * @param f Pointer to the forest to destroy.
 */
void tree_destroy_forest(tree_forest_t *f);

#endif /* BINARY_TREES_H */