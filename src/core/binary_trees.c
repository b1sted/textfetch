/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include "binary_trees.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sys_utils.h"

tree_node_t *tree_create_node(uint16_t id, const char *name)
{
    tree_node_t *new_node = NULL;

    if (!name) {
        goto cleanup;
    }

    new_node = malloc(sizeof(*new_node));
    if (!new_node) {
        V_PRINTF("[ERROR] Unable to allocate memory: %s\n", strerror(errno));
        goto cleanup;
    }

    new_node->name = strdup(name);
    if (!new_node->name) {
        V_PRINTF("[ERROR] Unable to allocate memory: %s\n", strerror(errno));
        goto cleanup;
    }

    new_node->id = id;
    new_node->first_child = NULL;
    new_node->next_sibling = NULL;

    return new_node;

cleanup:
    free(new_node);
    return NULL;
}

tree_forest_t *tree_create_forest(size_t initial_capacity)
{
    tree_forest_t *f = NULL;

    if (initial_capacity == 0) {
        initial_capacity = 1;
    }

    if (initial_capacity > SIZE_MAX / sizeof(tree_node_t *)) {
        V_PRINTF("[ERROR] Forest capacity is too large\n");
        goto cleanup;
    }

    f = malloc(sizeof(*f));
    if (!f) {
        V_PRINTF("[ERROR] Unable to allocate memory: %s\n", strerror(errno));
        goto cleanup;
    }

    f->count = 0;
    f->capacity = initial_capacity;
    f->roots = malloc(f->capacity * sizeof(tree_node_t *));
    if (!f->roots) {
        V_PRINTF("[ERROR] Unable to allocate memory: %s\n", strerror(errno));
        goto cleanup;
    }

    return f;

cleanup:
    if (f) {
        free(f->roots);
        free(f);
    }
    return NULL;
}

void tree_add_child(tree_node_t *parent, uint16_t id, const char *name)
{
    if (!parent) {
        return;
    }

    tree_node_t *new_node = tree_create_node(id, name);
    if (!new_node) {
        V_PRINTF("[ERROR] Failed to add child node (id: %" PRIu16 ", name: %s)\n", id, name);
        return;
    }

    if (parent->first_child == NULL) {
        parent->first_child = new_node;
    } else {
        tree_node_t *temp = parent->first_child;

        while (temp->next_sibling != NULL) {
            temp = temp->next_sibling;
        }

        temp->next_sibling = new_node;
    }
}

void tree_add_to_forest(tree_forest_t *f, tree_node_t *root)
{
    tree_node_t **new_roots = NULL;
    size_t new_capacity = 0;

    if (!f || !root) {
        goto cleanup;
    }

    if (f->count >= f->capacity) {
        if (f->capacity == 0) {
            new_capacity = 1;
        } else if (f->capacity > SIZE_MAX / 2) {
            V_PRINTF("[ERROR] Forest capacity overflow\n");
            goto cleanup;
        } else {
            new_capacity = f->capacity * 2;
        }

        if (new_capacity > SIZE_MAX / sizeof(tree_node_t *)) {
            V_PRINTF("[ERROR] Forest capacity is too large\n");
            goto cleanup;
        }

        new_roots = realloc(f->roots, new_capacity * sizeof(tree_node_t *));
        if (!new_roots) {
            V_PRINTF("[ERROR] Unable to allocate memory: %s\n", strerror(errno));
            goto cleanup;
        }

        f->roots = new_roots;
        f->capacity = new_capacity;
    }

    f->roots[f->count] = root;
    f->count++;

cleanup:
    return;
}

const tree_node_t *tree_find(const tree_node_t *root, uint16_t target_id)
{
    if (root == NULL) {
        return NULL;
    }

    if (root->id == target_id) {
        return root;
    }

    const tree_node_t *found = tree_find(root->first_child, target_id);
    if (found) {
        return found;
    }

    return tree_find(root->next_sibling, target_id);
}

tree_node_t *tree_find_root(const tree_forest_t *f, uint16_t target_id)
{
    if (!f) {
        return NULL;
    }

    for (size_t i = 0; i < f->count; i++) {
        if (f->roots[i]->id == target_id) {
            return f->roots[i];
        }
    }

    V_PRINTF("[NOTE] find_in_forest: Target Root ID 0x%04" PRIX16 " not found in forest.\n",
             target_id);
    return NULL;
}

void tree_free(tree_node_t *root)
{
    if (!root) {
        return;
    }

    while (root != NULL) {
        tree_node_t *next = root->next_sibling;
        if (root->first_child) {
            tree_free(root->first_child);
        }

        if (root->name) {
            free(root->name);
        }
        free(root);
        root = next;
    }
}

void tree_destroy_forest(tree_forest_t *f)
{
    if (!f) {
        return;
    }

    for (size_t i = 0; i < f->count; i++) {
        tree_free(f->roots[i]);
    }

    free(f->roots);
    free(f);
}
