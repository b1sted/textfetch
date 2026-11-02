/* SPDX-License-Identifier: MIT */

#include <limits.h>
#include <string.h>

#include "binary_trees.h"
#include "../support/test_framework.h"

static void test_create_node(void) {
    char source[] = "root";
    node *root = create_node(7, source);

    TEST_CHECK(root != NULL);
    if (!root) return;

    TEST_INT_EQ(root->id, 7);
    TEST_CHECK(root->name != source);
    TEST_STR_EQ(root->name, "root");
    TEST_CHECK(root->first_child == NULL);
    TEST_CHECK(root->next_sibling == NULL);

    source[0] = 'X';
    TEST_STR_EQ(root->name, "root");

    free_tree(root);

    TEST_CHECK(create_node(1, NULL) == NULL);

    node *empty = create_node(0, "");
    TEST_CHECK(empty != NULL);
    TEST_STR_EQ(empty->name, "");
    free_tree(empty);
}

static void test_create_forest(void) {
    forest *f = create_forest(2);
    TEST_CHECK(f != NULL);
    TEST_INT_EQ(f->count, 0);
    TEST_INT_EQ(f->capacity, 2);
    TEST_CHECK(f->roots != NULL);

    TEST_CHECK(find_in_forest(f, 1) == NULL);
    destroy_forest(f);
}

static void test_create_forest_zero_capacity(void) {
    forest *f = create_forest(0);
    TEST_CHECK(f != NULL);
    if (!f) return;

    TEST_INT_EQ(f->count, 0);
    TEST_CHECK(f->capacity >= 1);
    TEST_CHECK(f->roots != NULL);

    node *root = create_node(1, "root");
    TEST_CHECK(root != NULL);
    add_tree_to_forest(f, root);
    TEST_INT_EQ(f->count, 1);
    TEST_CHECK(f->roots[0] == root);
    destroy_forest(f);

    f = create_forest(-4);
    TEST_CHECK(f != NULL);
    if (!f) return;

    TEST_INT_EQ(f->count, 0);
    TEST_CHECK(f->capacity >= 1);
    TEST_CHECK(f->roots != NULL);
    destroy_forest(f);
}

static void test_add_child(void) {
    node *root = create_node(1, "root");
    TEST_CHECK(root != NULL);
    if (!root) return;

    add_child(root, 10, "first");
    add_child(root, 11, "second");
    add_child(root, 12, "third");

    TEST_CHECK(root->first_child != NULL);
    TEST_INT_EQ(root->first_child->id, 10);
    TEST_STR_EQ(root->first_child->name, "first");
    TEST_CHECK(root->first_child->next_sibling != NULL);
    TEST_INT_EQ(root->first_child->next_sibling->id, 11);
    TEST_CHECK(root->first_child->next_sibling->next_sibling != NULL);
    TEST_INT_EQ(root->first_child->next_sibling->next_sibling->id, 12);
    TEST_CHECK(root->first_child->next_sibling->next_sibling->next_sibling == NULL);

    add_child(NULL, 20, "orphan");
    add_child(root, 13, NULL);
    TEST_CHECK(root->first_child->next_sibling->next_sibling->next_sibling == NULL);

    free_tree(root);
}

static void test_forest_growth(void) {
    forest *f = create_forest(1);
    TEST_CHECK(f != NULL);
    if (!f) return;

    node *roots[5];
    for (int i = 0; i < 5; i++) {
        roots[i] = create_node((uint16_t)(i + 1), "root");
        add_tree_to_forest(f, roots[i]);
    }

    TEST_INT_EQ(f->count, 5);
    TEST_CHECK(f->capacity >= 5);

    for (int i = 0; i < 5; i++) {
        TEST_CHECK(f->roots[i] == roots[i]);
    }

    destroy_forest(f);
}

static void test_forest_growth_overflow_guard(void) {
    forest *f = create_forest(1);
    TEST_CHECK(f != NULL);
    if (!f) return;

    node *root = create_node(1, "root");
    TEST_CHECK(root != NULL);
    if (!root) {
        destroy_forest(f);
        return;
    }

    add_tree_to_forest(f, root);
    TEST_INT_EQ(f->count, 1);

    node **roots_before = f->roots;
    int capacity_before = f->capacity;

    f->capacity = INT_MAX;
    f->count = INT_MAX;

    node *extra = create_node(2, "extra");
    TEST_CHECK(extra != NULL);
    if (extra) {
        add_tree_to_forest(f, extra);
        TEST_INT_EQ(f->capacity, INT_MAX);
        TEST_INT_EQ(f->count, INT_MAX);
        TEST_CHECK(f->roots == roots_before);
        free_tree(extra);
    }

    f->count = 1;
    f->capacity = capacity_before;
    destroy_forest(f);
}

static void test_find_in_tree(void) {
    node *root = create_node(1, "root");
    if (!root) {
        TEST_CHECK(root != NULL);
        return;
    }
    add_child(root, 10, "child-a");
    add_child(root, 11, "child-b");
    add_child(root->first_child, 100, "grandchild");

    TEST_CHECK(find_in_tree(root, 1) == root);
    TEST_CHECK(find_in_tree(root, 10) == root->first_child);
    TEST_CHECK(find_in_tree(root, 11) == root->first_child->next_sibling);
    TEST_CHECK(find_in_tree(root, 100) == root->first_child->first_child);
    TEST_CHECK(find_in_tree(root, 999) == NULL);
    TEST_CHECK(find_in_tree(NULL, 1) == NULL);
    TEST_CHECK(find_in_tree(root->first_child, 11) ==
               root->first_child->next_sibling);

    free_tree(root);
}

static void test_find_in_forest(void) {
    forest *f = create_forest(4);
    if (!f) {
        TEST_CHECK(f != NULL);
        return;
    }
    node *first = create_node(1, "first");
    node *second = create_node(2, "second");

    add_child(second, 20, "nested");
    add_tree_to_forest(f, first);
    add_tree_to_forest(f, second);

    TEST_CHECK(find_in_forest(f, 1) == first);
    TEST_CHECK(find_in_forest(f, 2) == second);
    TEST_CHECK(find_in_forest(f, 20) == NULL);
    TEST_CHECK(find_in_forest(f, 999) == NULL);
    TEST_CHECK(find_in_forest(NULL, 1) == NULL);

    destroy_forest(f);
}

static void test_invalid_arguments(void) {
    node *root = create_node(1, "root");
    if (!root) {
        TEST_CHECK(root != NULL);
        return;
    }

    add_tree_to_forest(NULL, root);
    add_tree_to_forest(NULL, NULL);
    free_tree(root);

    free_tree(NULL);
    destroy_forest(NULL);
}

int main(void) {
    test_create_node();
    test_create_forest();
    test_create_forest_zero_capacity();
    test_add_child();
    test_forest_growth();
    test_forest_growth_overflow_guard();
    test_find_in_tree();
    test_find_in_forest();
    test_invalid_arguments();

    return test_finish("binary_trees");
}
