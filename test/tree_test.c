/// Tests for the various tree implementations.
/// NOTE I only free memory on the successful paths.
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "tree/naive_tree.h"
#include "tree/sleator_tree.h"

#include "test/mytester.h"

// NOTE There are 100 randomly shuffled keys in the range 0..=99. They were
//      generated with the Python script:
//      import random; x = list(range(100)); random.shuffle(x); print(x)
const KeyType random_keys_0[100] = {
    92, 31, 29, 49, 72, 95, 70, 13, 56, 33, 23, 27, 2,  76, 60, 19, 32, 54, 88, 89,
    30, 59, 80, 79, 34, 42, 65, 74, 69, 98, 17, 48, 26, 4,  28, 50, 96, 5,  1,  99,
    62, 52, 58, 73, 66, 10, 37, 90, 18, 3,  94, 7,  57, 82, 38, 35, 40, 21, 9,  51,
    77, 75, 16, 84, 43, 45, 91, 36, 46, 71, 22, 97, 93, 64, 53, 20, 24, 44, 8,  12,
    67, 14, 78, 87, 15, 63, 86, 68, 61, 11, 55, 47, 6,  39, 41, 81, 85, 25, 0,  83};
const KeyType random_keys_1[100] = {
    57, 59, 91, 50, 80, 51, 55, 93, 79, 2,  14, 4,  10, 46, 89, 73, 62, 32, 48, 64,
    23, 49, 31, 88, 36, 85, 12, 58, 26, 15, 97, 27, 90, 13, 52, 1,  30, 53, 68, 42,
    92, 71, 40, 66, 77, 17, 86, 35, 60, 63, 20, 29, 9,  22, 98, 96, 19, 45, 8,  5,
    25, 18, 69, 78, 33, 0,  54, 70, 83, 94, 28, 65, 47, 75, 95, 39, 38, 41, 87, 24,
    11, 43, 34, 3,  67, 16, 37, 61, 81, 99, 84, 82, 72, 6,  74, 7,  44, 76, 21, 56};
const KeyType random_keys_2[100] = {
    92, 35, 40, 99, 37, 32, 11, 28, 36, 84, 47, 50, 86, 73, 77, 64, 90, 17, 15, 69,
    42, 5,  25, 59, 29, 23, 52, 96, 70, 46, 34, 74, 67, 60, 30, 93, 22, 97, 89, 76,
    16, 13, 68, 20, 21, 78, 14, 4,  62, 98, 24, 63, 51, 85, 80, 49, 41, 75, 66, 55,
    91, 53, 10, 33, 81, 82, 79, 44, 71, 56, 39, 61, 87, 18, 58, 88, 31, 1,  9,  83,
    45, 2,  57, 19, 26, 38, 6,  27, 8,  72, 3,  0,  12, 7,  43, 94, 54, 65, 48, 95};
const KeyType random_keys_3[100] = {
    93, 22, 56, 72, 59, 98, 28, 43, 6,  82, 65, 88, 0,  30, 62, 26, 68, 7,  36, 9,
    90, 47, 52, 75, 50, 49, 13, 2,  71, 92, 44, 35, 95, 10, 37, 77, 58, 81, 5,  32,
    8,  70, 38, 20, 41, 17, 86, 60, 61, 78, 39, 80, 94, 34, 14, 79, 51, 67, 53, 21,
    76, 85, 99, 89, 73, 91, 15, 40, 69, 45, 83, 16, 27, 48, 74, 87, 96, 18, 42, 84,
    57, 54, 19, 25, 55, 12, 46, 4,  24, 63, 11, 23, 1,  97, 66, 3,  29, 33, 31, 64};

#define ASSERT_TRUE_OR_RETURN_FALSE(r, msg, tree)                                                  \
    do {                                                                                           \
        if (!(r)) {                                                                                \
            /* Clean up resources */                                                               \
            tree_free((tree));                                                                     \
            printf("[ERROR] %s:%d %s\n", __FILE__, __LINE__, (msg));                               \
            /* NOTE This assertion is for debugging purposes so that we have a                     \
            finer grain understanding of where the failure occurred. */                            \
            assert((r) && (msg));                                                                  \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

////////////////////////////////////////////////////////////////////////////////
/// NAIVE TREE TESTS
////////////////////////////////////////////////////////////////////////////////

static bool
manual_validation_test_for_naive()
{
    struct Tree *tree = tree_new();
    if (tree == NULL) {
        return false;
    }
    for (KeyType i = 0; i < 10; ++i) {
        bool r = tree_insert(tree, i);
        printf("Inserted " PRIu64 ": %d\n", (uint64_t)i, r);
        tree_print(tree);
    }
    for (KeyType i = 0; i < 11; ++i) {
        bool r = tree_search(tree, i);
        printf("Found " PRIu64 ": %d\n", (uint64_t)i, r);
    }
    for (KeyType i = 0; i < 11; ++i) {
        bool r = tree_remove(tree, i);
        printf("Removed " PRIu64 ": %d\n", (uint64_t)i, r);
        tree_print(tree);
    }
    tree_free(tree);
    return true;
}

static bool
random_test_for_naive()
{
    struct Tree *tree = tree_new();
    if (!tree) {
        return false;
    }

    for (uint64_t i = 0; i < 100; ++i) {
        KeyType key = random_keys_0[i];
        bool r = tree_insert(tree, key);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "insert should succeed", tree);
        r = tree_validate(tree);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "validation following insert should succeed", tree);
    }
    for (uint64_t i = 0; i < 100; ++i) {
        KeyType key = random_keys_0[i];
        bool r = tree_search(tree, key);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "search should succeed", tree);
        uint64_t reverse_rank = tree_reverse_rank(tree, key);
        if (reverse_rank != 99 - key) {
            // NOTE This assertion is for debugging purposes so that we have a
            //      finer grain understanding of where the failure occurred.
            printf("[ERROR] Key: " PRIu64 ", Got reverse rank: " PRIu64
                   ", Expected reverse rank: " PRIu64 "\n",
                   key,
                   reverse_rank,
                   99 - key);
            tree_free(tree);
            assert(0 && "reverse rank should be 99 - key for keys in range 0..=99");
            return false;
        }
        r = tree_validate(tree);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "validation following search should succeed", tree);
    }
    // NOTE We run the search a second time to ensure that it does not modify
    //      the tree.
    for (uint64_t i = 0; i < 100; ++i) {
        KeyType key = random_keys_0[i];
        bool r = tree_search(tree, key);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "search should succeed", tree);
        uint64_t reverse_rank = tree_reverse_rank(tree, key);
        if (reverse_rank != 99 - key) {
            // NOTE This assertion is for debugging purposes so that we have a
            //      finer grain understanding of where the failure occurred.
            printf("[ERROR] Key: " PRIu64 ", Got reverse rank: " PRIu64
                   ", Expected reverse rank: " PRIu64 "\n",
                   key,
                   reverse_rank,
                   99 - key);
            tree_free(tree);
            assert(0 && "reverse rank should be 99 - key for keys in range 0..=99");
            return false;
        }
        r = tree_validate(tree);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "validation following search should succeed", tree);
    }
    for (uint64_t i = 0; i < 100; ++i) {
        KeyType key = random_keys_0[i];
        bool r = tree_remove(tree, key);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "remove should succeed", tree);
        r = tree_validate(tree);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "validation following remove should succeed", tree);
    }
    tree_free(tree);
    return true;
}

static bool
random_test_with_different_traces_for_naive()
{
    struct Tree *tree = tree_new();
    if (!tree) {
        return false;
    }

    for (uint64_t i = 0; i < 100; ++i) {
        KeyType key = random_keys_0[i];
        bool r = tree_insert(tree, key);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "insert should succeed", tree);
        r = tree_validate(tree);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "validation following insert should succeed", tree);
    }
    for (uint64_t i = 0; i < 100; ++i) {
        KeyType key = random_keys_1[i];
        bool r = tree_search(tree, key);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "search should succeed", tree);
        uint64_t reverse_rank = tree_reverse_rank(tree, key);
        if (reverse_rank != 99 - key) {
            // NOTE This assertion is for debugging purposes so that we have a
            //      finer grain understanding of where the failure occurred.
            printf("[ERROR] Key: " PRIu64 ", Got reverse rank: " PRIu64
                   ", Expected reverse rank: " PRIu64 "\n",
                   key,
                   reverse_rank,
                   99 - key);
            assert(0 && "reverse rank should be 99 - key for keys in range 0..=99");
            return false;
        }
        r = tree_validate(tree);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "validation following search should succeed", tree);
    }
    // NOTE We run the search a second time to ensure that it does not modify
    //      the tree.
    for (uint64_t i = 0; i < 100; ++i) {
        KeyType key = random_keys_2[i];
        bool r = tree_search(tree, key);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "search should succeed", tree);
        uint64_t reverse_rank = tree_reverse_rank(tree, key);
        if (reverse_rank != 99 - key) {
            // NOTE This assertion is for debugging purposes so that we have a
            //      finer grain understanding of where the failure occurred.
            printf("[ERROR] Key: " PRIu64 ", Got reverse rank: " PRIu64
                   ", Expected reverse rank: " PRIu64 "\n",
                   key,
                   reverse_rank,
                   99 - key);
            assert(0 && "reverse rank should be 99 - key for keys in range 0..=99");
            return false;
        }
        r = tree_validate(tree);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "validation following search should succeed", tree);
    }
    for (uint64_t i = 0; i < 100; ++i) {
        KeyType key = random_keys_3[i];
        bool r = tree_remove(tree, key);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "remove should succeed", tree);
        r = tree_validate(tree);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "validation following remove should succeed", tree);
    }
    tree_free(tree);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
/// SLEATOR SPLAY TREE TESTS
////////////////////////////////////////////////////////////////////////////////

static bool
random_test_with_different_traces_for_sleator()
{
    struct Tree *tree = tree_new();
    bool r = false;
    if (!tree) {
        return false;
    }

    for (uint64_t i = 0; i < 100; ++i) {
        KeyType key = random_keys_0[i];
        r = sleator_insert(tree, key);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "insert should succeed", tree);
        r = tree_validate(tree);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "validation following insert should succeed", tree);
    }
    for (uint64_t i = 0; i < 100; ++i) {
        KeyType key = random_keys_1[i];
        uint64_t reverse_rank = tree_reverse_rank(tree, key);
        if (reverse_rank != 99 - key) {
            // NOTE This assertion is for debugging purposes so that we have a
            //      finer grain understanding of where the failure occurred.
            printf("[ERROR] Key: " PRIu64 ", Got reverse rank: " PRIu64
                   ", Expected reverse rank: " PRIu64 "\n",
                   key,
                   reverse_rank,
                   99 - key);
            assert(0 && "reverse rank should be 99 - key for keys in range 0..=99");
            return false;
        }
        r = tree_validate(tree);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "validation following search should succeed", tree);
    }
    // NOTE We run the search a second time to ensure that it does not modify
    //      the tree.
    for (uint64_t i = 0; i < 100; ++i) {
        KeyType key = random_keys_2[i];
        uint64_t reverse_rank = tree_reverse_rank(tree, key);
        if (reverse_rank != 99 - key) {
            // NOTE This assertion is for debugging purposes so that we have a
            //      finer grain understanding of where the failure occurred.
            printf("[ERROR] Key: " PRIu64 ", Got reverse rank: " PRIu64
                   ", Expected reverse rank: " PRIu64 "\n",
                   key,
                   reverse_rank,
                   99 - key);
            assert(0 && "reverse rank should be 99 - key for keys in range 0..=99");
            return false;
        }
        r = tree_validate(tree);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "validation following search should succeed", tree);
    }
    for (uint64_t i = 0; i < 100; ++i) {
        KeyType key = random_keys_3[i];
        bool r = sleator_remove(tree, key);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "remove should succeed", tree);
        r = tree_validate(tree);
        ASSERT_TRUE_OR_RETURN_FALSE(r, "validation following remove should succeed", tree);
    }
    tree_free(tree);
    return true;
}

int
main()
{
    bool manual_tests = false;
    if (manual_tests) {
        // These tests require manual inspection. Ugh!
        ASSERT_FUNCTION_RETURNS_TRUE(manual_validation_test_for_naive());
    }

    // These are automatic tests for the naive tree
    ASSERT_FUNCTION_RETURNS_TRUE(random_test_for_naive());
    ASSERT_FUNCTION_RETURNS_TRUE(random_test_with_different_traces_for_naive());

    // Automatic tests for Sleator's tree
    ASSERT_FUNCTION_RETURNS_TRUE(random_test_with_different_traces_for_sleator());

    return EXIT_SUCCESS;
}
