/// @brief  This file provides the basic functionality for a Binary Search Tree
///         with the Order Statistic Tree functionality.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tree/types.h"

struct Subtree *
subtree__new(KeyType key);

bool
tree__init(struct Tree *me);

struct Tree *
tree__new(void);

/// @brief  Get the number of elements in the tree.
uint64_t
tree__cardinality(struct Tree *me);

bool
subtree__insert(struct Subtree *me, KeyType key);

bool
tree__insert(struct Tree *me, KeyType key);

bool
tree__search(struct Tree *me, KeyType key);

/// @brief  Get the reverse-order statistic tree rank. That is the order
///         statistic tree rank but measured from the right (i.e. from the max).
///         The regular rank would be the (total_tree__size - reverse_rank - 1).
///
/// @return If the element is not found, we return SIZE_MAX. Otherwise the
/// reverse rank.
uint64_t
tree__reverse_rank(struct Tree *me, KeyType key);

bool
tree__remove(struct Tree *me, KeyType key);

/// @brief  Print as a JSON (for sticking in JSON graphical software).
void
tree__print(struct Tree *me);

/// @brief  Pretty print semi-readably to the screen.
void
tree__prettyprint(struct Tree *me);

/// @brief  Validate the correctness of the tree
bool
tree__validate(struct Tree *me);

void
subtree__free(struct Subtree *me, size_t const recursion_depth);

/// @brief  Free the structures within the tree without freeing the tree itself.
///         Useful if we allocated the tree on the stack.
void
tree__destroy(struct Tree *me);

void
tree__free(struct Tree *me);
