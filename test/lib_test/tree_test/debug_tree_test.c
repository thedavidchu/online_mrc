#include <stddef.h>
#include <stdint.h>

#include "logger/logger.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "tree/types.h"

// NOTE There are 100 randomly shuffled keys in the range 0..=99. They were
//      generated with the Python script:
//      import random; x = list(range(100)); random.shuffle(x); print(x)
const KeyType random_keys_0[100] = {
    92, 31, 29, 49, 72, 95, 70, 13, 56, 33, 23, 27, 2,  76, 60, 19, 32,
    54, 88, 89, 30, 59, 80, 79, 34, 42, 65, 74, 69, 98, 17, 48, 26, 4,
    28, 50, 96, 5,  1,  99, 62, 52, 58, 73, 66, 10, 37, 90, 18, 3,  94,
    7,  57, 82, 38, 35, 40, 21, 9,  51, 77, 75, 16, 84, 43, 45, 91, 36,
    46, 71, 22, 97, 93, 64, 53, 20, 24, 44, 8,  12, 67, 14, 78, 87, 15,
    63, 86, 68, 61, 11, 55, 47, 6,  39, 41, 81, 85, 25, 0,  83};

static bool
debug_sleator_tree(size_t const size)
{
    struct Tree tree = {0};
    if (!tree__init(&tree)) {
        LOGGER_ERROR("");
        return false;
    }
    for (size_t i = 0; i < size; ++i) {
        tree__sleator_insert(&tree, random_keys_0[i]);
        tree__prettyprint(&tree);
        printf("---\n");
    }
    tree__destroy(&tree);
    return true;
}

int
main(void)
{
    debug_sleator_tree(5);
    return 0;
}
