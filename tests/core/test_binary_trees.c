/* SPDX-License-Identifier: MIT */

#include "binary_trees.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "../support/test_framework.h"

static void test_create_node(void)
{
    char source[] = "root";
    tree_node_t *root = tree_create_node(7, source);

    TEST_CHECK(root != NULL);
    if (!root) {
        return;
    }

    TEST_INT_EQ(root->id, 7);
    TEST_CHECK(root->name != source);
    TEST_STR_EQ(root->name, "root");
    TEST_CHECK(root->first_child == NULL);
    TEST_CHECK(root->next_sibling == NULL);

    source[0] = 'X';
    TEST_STR_EQ(root->name, "root");

    tree_free(root);

    TEST_CHECK(tree_create_node(1, NULL) == NULL);

    tree_node_t *empty = tree_create_node(0, "");
    TEST_CHECK(empty != NULL);
    TEST_STR_EQ(empty->name, "");
    tree_free(empty);
}

static void test_create_forest(void)
{
    tree_forest_t *f = tree_create_forest(2);
    TEST_CHECK(f != NULL);
    TEST_INT_EQ(f->count, 0);
    TEST_INT_EQ(f->capacity, 2);
    TEST_CHECK(f->roots != NULL);

    TEST_CHECK(tree_find_root(f, 1) == NULL);
    tree_destroy_forest(f);
}

static void test_create_forest_zero_capacity(void)
{
    tree_forest_t *f = tree_create_forest(0);
    TEST_CHECK(f != NULL);
    if (!f) {
        return;
    }

    TEST_INT_EQ(f->count, 0);
    TEST_CHECK(f->capacity >= 1);
    TEST_CHECK(f->roots != NULL);

    tree_node_t *root = tree_create_node(1, "root");
    TEST_CHECK(root != NULL);
    tree_add_to_forest(f, root);
    TEST_INT_EQ(f->count, 1);
    TEST_CHECK(f->roots[0] == root);
    tree_destroy_forest(f);

    f = tree_create_forest(SIZE_MAX);
    TEST_CHECK(f == NULL);
}

static void test_add_child(void)
{
    tree_node_t *root = tree_create_node(1, "root");
    TEST_CHECK(root != NULL);
    if (!root) {
        return;
    }

    tree_add_child(root, 10, "first");
    tree_add_child(root, 11, "second");
    tree_add_child(root, 12, "third");

    TEST_CHECK(root->first_child != NULL);
    TEST_INT_EQ(root->first_child->id, 10);
    TEST_STR_EQ(root->first_child->name, "first");
    TEST_CHECK(root->first_child->next_sibling != NULL);
    TEST_INT_EQ(root->first_child->next_sibling->id, 11);
    TEST_CHECK(root->first_child->next_sibling->next_sibling != NULL);
    TEST_INT_EQ(root->first_child->next_sibling->next_sibling->id, 12);
    TEST_CHECK(root->first_child->next_sibling->next_sibling->next_sibling == NULL);

    tree_add_child(NULL, 20, "orphan");
    tree_add_child(root, 13, NULL);
    TEST_CHECK(root->first_child->next_sibling->next_sibling->next_sibling == NULL);

    tree_free(root);
}

static void test_forest_growth(void)
{
    tree_forest_t *f = tree_create_forest(1);
    TEST_CHECK(f != NULL);
    if (!f) {
        return;
    }

    tree_node_t *roots[5];
    for (int i = 0; i < 5; i++) {
        roots[i] = tree_create_node((uint16_t)(i + 1), "root");
        tree_add_to_forest(f, roots[i]);
    }

    TEST_INT_EQ(f->count, 5);
    TEST_CHECK(f->capacity >= 5);

    for (int i = 0; i < 5; i++) {
        TEST_CHECK(f->roots[i] == roots[i]);
    }

    tree_destroy_forest(f);
}

static void test_forest_growth_overflow_guard(void)
{
    tree_forest_t *f = tree_create_forest(1);
    TEST_CHECK(f != NULL);
    if (!f) {
        return;
    }

    tree_node_t *root = tree_create_node(1, "root");
    TEST_CHECK(root != NULL);
    if (!root) {
        tree_destroy_forest(f);
        return;
    }

    tree_add_to_forest(f, root);
    TEST_INT_EQ(f->count, 1);

    tree_node_t **roots_before = f->roots;
    size_t capacity_before = f->capacity;
    size_t oversized_capacity = SIZE_MAX / sizeof(tree_node_t *) + 1;

    f->capacity = oversized_capacity;
    f->count = oversized_capacity;

    tree_node_t *extra = tree_create_node(2, "extra");
    TEST_CHECK(extra != NULL);
    if (extra) {
        tree_add_to_forest(f, extra);
        TEST_INT_EQ(f->capacity, oversized_capacity);
        TEST_INT_EQ(f->count, oversized_capacity);
        TEST_CHECK(f->roots == roots_before);
        tree_free(extra);
    }

    f->count = 1;
    f->capacity = capacity_before;
    tree_destroy_forest(f);
}

static void test_find_in_tree(void)
{
    tree_node_t *root = tree_create_node(1, "root");
    if (!root) {
        TEST_CHECK(root != NULL);
        return;
    }
    tree_add_child(root, 10, "child-a");
    tree_add_child(root, 11, "child-b");
    tree_add_child(root->first_child, 100, "grandchild");

    TEST_CHECK(tree_find(root, 1) == root);
    TEST_CHECK(tree_find(root, 10) == root->first_child);
    TEST_CHECK(tree_find(root, 11) == root->first_child->next_sibling);
    TEST_CHECK(tree_find(root, 100) == root->first_child->first_child);
    TEST_CHECK(tree_find(root, 999) == NULL);
    TEST_CHECK(tree_find(NULL, 1) == NULL);
    TEST_CHECK(tree_find(root->first_child, 11) == root->first_child->next_sibling);

    tree_free(root);
}

static void test_find_in_forest(void)
{
    tree_forest_t *f = tree_create_forest(4);
    if (!f) {
        TEST_CHECK(f != NULL);
        return;
    }
    tree_node_t *first = tree_create_node(1, "first");
    tree_node_t *second = tree_create_node(2, "second");

    tree_add_child(second, 20, "nested");
    tree_add_to_forest(f, first);
    tree_add_to_forest(f, second);

    TEST_CHECK(tree_find_root(f, 1) == first);
    TEST_CHECK(tree_find_root(f, 2) == second);
    TEST_CHECK(tree_find_root(f, 20) == NULL);
    TEST_CHECK(tree_find_root(f, 999) == NULL);
    TEST_CHECK(tree_find_root(NULL, 1) == NULL);

    tree_destroy_forest(f);
}

static void test_invalid_arguments(void)
{
    tree_node_t *root = tree_create_node(1, "root");
    if (!root) {
        TEST_CHECK(root != NULL);
        return;
    }

    tree_add_to_forest(NULL, root);
    tree_add_to_forest(NULL, NULL);
    tree_free(root);

    tree_free(NULL);
    tree_destroy_forest(NULL);
}

int main(void)
{
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
