#pragma once
/*
           An implementation of top-down splaying with sizes
             D. Sleator <sleator@cs.cmu.edu>, January 1994.
Modified a little by Qingpeng Niu for tracing the global chunck library memory use.
Just add a compute sum of size from search node to the right most node.

Modified a bit by David Chu. Refactored some bits to make the code more useable.
*/
#include <stdbool.h>
#include <stdint.h>

#include "tree/naive_tree.h"

bool
tree__sleator_insert(struct Tree *t, KeyType key);

bool
tree_sleator_remove(struct Tree *t, KeyType key);

struct Subtree *
sleator_find_rank(struct Subtree *t, unsigned rank);
