/// @brief  This file extends the naive_tree.h file by adding inserts and remove
///         operations that use the Splay operation.
///         NOTE    Splay trees do not require additional storage overhead
///                 compared to non-balancing Binary Search Trees.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tree/naive_tree.h"

bool
tree__splayed_insert(struct Tree *me, KeyType key);

bool
tree__splayed_remove(struct Tree *me, KeyType key);
