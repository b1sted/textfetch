/* SPDX-License-Identifier: MIT */

#ifndef BINARY_TREES_H
#define BINARY_TREES_H

#include <stdint.h>

typedef struct node {
    uint16_t id;
    char *name;
    struct node *first_child;
    struct node *next_sibling;
} node;

typedef struct {
    node **roots;
    int count;
    int capacity;
} forest;

node *create_node(uint16_t id, char *name);
forest *create_forest(int initial_capacity);

void add_child(node *parent, uint16_t id, char *name);
void add_tree_to_forest(forest *f, node *root);

node *find_in_tree(node *root, uint16_t target_id);
node *find_in_forest(forest *f, uint16_t target_id);

void free_tree(node *root);
void destroy_forest(forest *f);

#endif /* BINARY_TREES_H */