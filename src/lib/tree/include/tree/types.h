#pragma once

#include <stdint.h>

#include "types/key_type.h"

struct Tree;
struct Subtree;

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
