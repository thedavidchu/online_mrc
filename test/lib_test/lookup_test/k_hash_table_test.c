#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "logger/logger.h"
#include "lookup/k_hash_table.h"

#include "lookup/lookup.h"
#include "test/mytester.h"

bool
test_khash(void)
{
    struct KHashTable me = {0};
    g_assert_true(KHashTable__init(&me));
    g_assert_true(KHashTable__write(&me, stdout, true));

    // Test successful lookups
    for (uint64_t i = 0; i < 10; ++i) {
        enum PutUniqueStatus r = KHashTable__put_unique(&me, i, 2 * i);
        g_assert_cmpint(r, ==, LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE);
        KHashTable__write(&me, stdout, true);
    }

    // Test successful lookups
    for (uint64_t i = 0; i < 10; ++i) {
        struct LookupReturn r = KHashTable__lookup(&me, i);
        g_assert_true(r.success);
        g_assert_cmpuint(r.timestamp, ==, 2 * i);
    }

    // Test unsuccessful lookups
    for (uint64_t i = 10; i < 20; ++i) {
        struct LookupReturn r = KHashTable__lookup(&me, i);
        g_assert_false(r.success);
    }

    // Test successful replacements
    for (uint64_t i = 0; i < 10; ++i) {
        enum PutUniqueStatus r = KHashTable__put_unique(&me, i, 3 * i);
        g_assert_cmpint(r, ==, LOOKUP_PUTUNIQUE_REPLACE_VALUE);
        KHashTable__write(&me, stdout, true);
    }

    // Test successful lookups
    for (uint64_t i = 0; i < 10; ++i) {
        struct LookupReturn r = KHashTable__lookup(&me, i);
        g_assert_true(r.success);
        g_assert_cmpuint(r.timestamp, ==, 3 * i);
    }

    // Test unsuccessful lookups
    for (uint64_t i = 10; i < 20; ++i) {
        struct LookupReturn r = KHashTable__lookup(&me, i);
        g_assert_false(r.success);
    }

    // Test successful deletes
    for (uint64_t i = 0; i < 10; ++i) {
        struct LookupReturn r = KHashTable__remove(&me, i);
        g_assert_true(r.success);
        g_assert_cmpuint(r.timestamp, ==, 3 * i);
        KHashTable__write(&me, stdout, true);
    }

    // Test unsuccessful deletes
    for (uint64_t i = 10; i < 20; ++i) {
        struct LookupReturn r = KHashTable__remove(&me, i);
        g_assert_false(r.success);
    }

    KHashTable__write(&me, stdout, true);
    KHashTable__destroy(&me);
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_khash());
    return EXIT_SUCCESS;
}
