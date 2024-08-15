#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "lookup/k_hash_table.h"
#include "lookup/lookup.h"
#include "test/mytester.h"
#include "unused/mark_unused.h"

#define MAX_SIZE         (1 << 20)
#define STREAM           NULL
#define MAX_INT_PLUS_ONE (((size_t)1 << 32) + 1)

static bool
test_khash(void)
{
    struct KHashTable me = {0};
    g_assert_true(KHashTable__init(&me));
    KHashTable__write(&me, STREAM, true);

    // Test successful inserts
    for (uint64_t i = 0; i < MAX_SIZE; ++i) {
        enum PutUniqueStatus r = KHashTable__put(&me, i, 2 * i);
        g_assert_cmpint(r, ==, LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE);
        KHashTable__write(&me, STREAM, true);
    }

    // Test successful lookups
    for (uint64_t i = 0; i < MAX_SIZE; ++i) {
        struct LookupReturn r = KHashTable__lookup(&me, i);
        g_assert_true(r.success);
        g_assert_cmpuint(r.timestamp, ==, 2 * i);
    }

    // Test unsuccessful lookups
    for (uint64_t i = MAX_SIZE; i < MAX_SIZE + 10; ++i) {
        struct LookupReturn r = KHashTable__lookup(&me, i);
        g_assert_false(r.success);
    }

    // Test successful replacements
    for (uint64_t i = 0; i < MAX_SIZE; ++i) {
        enum PutUniqueStatus r = KHashTable__put(&me, i, 3 * i);
        g_assert_cmpint(r, ==, LOOKUP_PUTUNIQUE_REPLACE_VALUE);
        KHashTable__write(&me, STREAM, true);
    }

    // Test successful lookups
    for (uint64_t i = 0; i < MAX_SIZE; ++i) {
        struct LookupReturn r = KHashTable__lookup(&me, i);
        g_assert_true(r.success);
        g_assert_cmpuint(r.timestamp, ==, 3 * i);
    }

    // Test unsuccessful lookups
    for (uint64_t i = MAX_SIZE; i < MAX_SIZE + 10; ++i) {
        struct LookupReturn r = KHashTable__lookup(&me, i);
        g_assert_false(r.success);
    }

    // Test successful deletes
    for (uint64_t i = 0; i < MAX_SIZE; ++i) {
        struct LookupReturn r = KHashTable__remove(&me, i);
        g_assert_true(r.success);
        g_assert_cmpuint(r.timestamp, ==, 3 * i);
        KHashTable__write(&me, STREAM, true);
    }

    // Test unsuccessful deletes
    for (uint64_t i = MAX_SIZE; i < MAX_SIZE + 10; ++i) {
        struct LookupReturn r = KHashTable__remove(&me, i);
        g_assert_false(r.success);
    }

    KHashTable__write(&me, STREAM, true);
    KHashTable__destroy(&me);
    return true;
}

/// @brief  Test the ability to store more than 4 billion elements.
static bool
test_large_khash(void)
{
    struct KHashTable me = {0};
    g_assert_true(KHashTable__init(&me));

    for (uint64_t i = 0; i < MAX_INT_PLUS_ONE; ++i) {
        enum PutUniqueStatus r = KHashTable__put(&me, i, 2 * i);
        g_assert_cmpint(r, ==, LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE);
    }

    KHashTable__destroy(&me);
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_khash());
    UNUSED(test_large_khash);
    return EXIT_SUCCESS;
}
