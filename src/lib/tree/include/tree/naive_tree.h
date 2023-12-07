#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef size_t KeyType;

struct NaiveTree;

struct NaiveTree *
tree_new();

/// @brief  Get the number of elements in the tree.
size_t
tree_cardinality(struct NaiveTree *me);

bool
tree_insert(struct NaiveTree *me, KeyType key);

bool
tree_search(struct NaiveTree *me, KeyType key);

bool
tree_remove(struct NaiveTree *me, KeyType key);