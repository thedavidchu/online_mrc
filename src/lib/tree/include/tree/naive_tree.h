/// @brief  This file provides the basic functionality for a Binary Search Tree
///         with the Order Statistic Tree functionality.

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t KeyType;

struct Tree;

struct Subtree {
    KeyType key;
    uint64_t cardinality;
    struct Subtree *left_subtree;
    struct Subtree *right_subtree;
};

struct Tree {
    struct Subtree *root;
    uint64_t cardinality;
};

struct Subtree *
subtree_new(KeyType key);

struct Tree *
tree_new();

/// @brief  Get the number of elements in the tree.
uint64_t
tree_cardinality(struct Tree *me);

bool
subtree_insert(struct Subtree *me, KeyType key);

bool
tree_insert(struct Tree *me, KeyType key);

bool
tree_search(struct Tree *me, KeyType key);

/// @brief  Get the reverse-order statistic tree rank. That is the order
///         statistic tree rank but measured from the right (i.e. from the max).
///         The regular rank would be the (total_tree_size - reverse_rank - 1).
///
/// @return If the element is not found, we return SIZE_MAX. Otherwise the reverse rank.
uint64_t
tree_reverse_rank(struct Tree *me, KeyType key);

bool
tree_remove(struct Tree *me, KeyType key);

/// @brief  Print as a JSON (for sticking in JSON graphical software).
void
tree_print(struct Tree *me);

/// @brief  Pretty print semi-readably to the screen.
void
tree_prettyprint(struct Tree *me);

/// @brief  Validate the correctness of the tree
bool
tree_validate(struct Tree *me);

void
subtree_free(struct Subtree *me);

void
tree_free(struct Tree *me);
