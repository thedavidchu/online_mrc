/// @brief  This file provides the basic functionality for a Binary Search Tree
///         with the Order Statistic Tree functionality.

#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef size_t KeyType;

struct Tree;

struct Tree *
tree_new();

/// @brief  Get the number of elements in the tree.
size_t
tree_cardinality(struct Tree *me);

bool
tree_naive_insert(struct Tree *me, KeyType key);

bool
tree_search(struct Tree *me, KeyType key);

/// @brief  Get the reverse-order statistic tree rank. That is the order
///         statistic tree rank but measured from the right (i.e. from the max).
///         The regular rank would be the (total_tree_size - reverse_rank - 1).
///
/// @return If the element is not found, we return SIZE_MAX. Otherwise the reverse rank.
size_t
tree_reverse_rank(struct Tree *me, KeyType key);

bool
tree_naive_remove(struct Tree *me, KeyType key);

void
tree_print(struct Tree *me);

/// @brief  Validate the correctness of the tree
bool
tree_validate(struct Tree *me);
