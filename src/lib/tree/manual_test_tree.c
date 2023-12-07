#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "tree/naive_tree.h"

static bool
manual_validation_test()
{
    struct NaiveTree *tree = tree_new();
    if (tree == NULL) {
        return false;
    }
    for (KeyType i = 0; i < 10; ++i) {
        bool r = tree_insert(tree, i);
        printf("Inserted %zu: %d\n", i, r);
        tree_print(tree);
    }
    for (KeyType i = 0; i < 11; ++i) {
        bool r = tree_search(tree, i);
        printf("Found %zu: %d\n", i, r);
    }
    for (KeyType i = 0; i < 11; ++i) {
        bool r = tree_remove(tree, i);
        printf("Removed %zu: %d\n", i, r);
        tree_print(tree);
    }
    return true;
}


static bool
random_test()
{
    // There are 100 random keys in the range 0..=99
    KeyType random_keys[] = {92, 31, 29, 49, 72, 95, 70, 13, 56, 33, 23, 27, 2, 76, 60, 19, 32, 54, 88, 89, 30, 59, 80, 79, 34, 42, 65, 74, 69, 98, 17, 48, 26, 4, 28, 50, 96, 5, 1, 99, 62, 52, 58, 73, 66, 10, 37, 90, 18, 3, 94, 7, 57, 82, 38, 35, 40, 21, 9, 51, 77, 75, 16, 84, 43, 45, 91, 36, 46, 71, 22, 97, 93, 64, 53, 20, 24, 44, 8, 12, 67, 14, 78, 87, 15, 63, 86, 68, 61, 11, 55, 47, 6, 39, 41, 81, 85, 25, 0, 83};
    struct NaiveTree *tree = tree_new();
    if (!tree) {
        return false;
    }

    for (size_t i = 0; i < 100; ++i) {
        KeyType key = random_keys[i];
        bool r = tree_insert(tree, key);
        if (!r) {
            // NOTE This assertion is for debugging purposes so that we have a
            //      finer grain understanding of where the failure occurred.
            assert(r && "insert should succeed");
            return false;
        }
        r = tree_validate(tree);
        if (!r) {
            // NOTE This assertion is for debugging purposes so that we have a
            //      finer grain understanding of where the failure occurred.
            assert(r && "validation following insert should succeed");
            return false;
        }
    }
    for (size_t i = 0; i < 100; ++i) {
        KeyType key = random_keys[i];
        bool r = tree_search(tree, key);
        if (!r) {
            // NOTE This assertion is for debugging purposes so that we have a
            //      finer grain understanding of where the failure occurred.
            assert(r && "search should succeed");
            return false;
        }
        r = tree_validate(tree);
        if (!r) {
            // NOTE This assertion is for debugging purposes so that we have a
            //      finer grain understanding of where the failure occurred.
            assert(r && "validation following search should succeed");
            return false;
        }
    }
    for (size_t i = 0; i < 100; ++i) {
        KeyType key = random_keys[i];
        tree_print(tree);
        bool r = tree_remove(tree, key);
        if (!r) {
            // NOTE This assertion is for debugging purposes so that we have a
            //      finer grain understanding of where the failure occurred.
            assert(r && "remove should succeed");
            return false;
        }
        r = tree_validate(tree);
        tree_print(tree);
        if (!r) {
            // NOTE This assertion is for debugging purposes so that we have a
            //      finer grain understanding of where the failure occurred.
            assert(r && "validation following remove should succeed");
            return false;
        }
    }
    return true;
}

int
main()
{
    bool r = false;
    bool manual_tests = false;
    if (manual_tests) {
        // These tests require manual inspection. Ugh!
        r = manual_validation_test();
        if (!r) {
            assert(0 && "manual_validation_test failed");
            exit(EXIT_FAILURE);
        }
    }

    // These are automatic tests
    r = random_test();
    if (!r) {
        assert(0 && "random_test failed");
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}
