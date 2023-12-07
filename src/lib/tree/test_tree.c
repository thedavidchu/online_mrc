#include <stdio.h>

#include "tree/naive_tree.h"

int
main()
{
    struct NaiveTree *tree = tree_new();

    for (KeyType i = 0; i < 10; ++i) {
        tree_insert(tree, i);
    }
    for (KeyType i = 0; i < 11; ++i) {
        bool r = tree_search(tree, i);
        printf("Found %zu: %d\n", i, r);
    }
    // TODO(dchu): test remove
    return 0;
}