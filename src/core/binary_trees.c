/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>

#include "binary_trees.h"
#include "sys_utils.h"

node *create_node(uint16_t id, char *name) {
    node *new_node = (node *)malloc(sizeof(node));
    if (!new_node) return NULL;

    new_node->id = id;
    new_node->name = strdup(name);
    new_node->first_child = NULL;
    new_node->next_sibling = NULL;

    return new_node;
}

forest *create_forest(int initial_capacity) {
    forest *f = malloc(sizeof(forest));

    f->count = 0;
    f->capacity = initial_capacity;
    f->roots = malloc(f->capacity * sizeof(node *));

    return f;
}

void add_child(node *parent, uint16_t id, char *name) {
    node *new_node = create_node(id, name);
    if (parent->first_child == NULL) {
        parent->first_child = new_node;
    } else {
        node *temp = parent->first_child;

        while (temp->next_sibling != NULL) {
            temp = temp->next_sibling;
        }

        temp->next_sibling = new_node;
    }
}

void add_tree_to_forest(forest *f, node *root) {
    if (f->count >= f->capacity) {
        f->capacity *= 2;

        node **new_roots = realloc(f->roots, f->capacity * sizeof(node *));
        if (!new_roots) {
            V_PRINTF("[ERROR] add_tree_to_forest: realloc failed\n");
            return;
        }

        f->roots = new_roots;
    }

    f->roots[f->count] = root;
    f->count++;
}

node *find_in_tree(node *root, uint16_t target_id) {
    if (root == NULL) return NULL;

    if (root->id == target_id) return root;

    node *found = find_in_tree(root->first_child, target_id);
    if (found) return found;

    return find_in_tree(root->next_sibling, target_id);
}

node *find_in_forest(forest *f, uint16_t target_id) {
    for (int i = 0; i < f->count; i++) {
        if (f->roots[i]->id == target_id) return f->roots[i];
    }

    V_PRINTF("[NOTE] find_in_forest: Target Root ID 0x%04X not found in forest.\n",
             target_id);
    return NULL;
}

void free_tree(node *root) {
    while (root != NULL) {
        node *next = root->next_sibling;
        if (root->first_child) {
            free_tree(root->first_child);
        }

        if (root->name)
            free(root->name);
        free(root);
        root = next;
    }
}

void destroy_forest(forest *f) {
    for (int i = 0; i < f->count; i++) {
        free_tree(f->roots[i]);
    }

    free(f->roots);
    free(f);
}
