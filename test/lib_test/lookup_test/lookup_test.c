#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#include <glib.h>

#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "test/mytester.h"

#define N 1000

bool
hash_table_test(void)
{
    struct HashTable me = {0};
    g_assert_true(HashTable__init(&me));

    for (size_t i = 0; i < N; ++i) {
        g_assert_true(HashTable__put(&me, i, i));
    }

    for (size_t i = 0; i < N; ++i) {
        struct LookupReturn r = HashTable__lookup(&me, i);
        g_assert_true(r.success == true && r.timestamp == i);
    }

    for (size_t i = 0; i < N; ++i) {
        g_assert_true(HashTable__put(&me, i, 1234567890));
    }

    for (size_t i = 0; i < N; ++i) {
        struct LookupReturn r = HashTable__lookup(&me, i);
        g_assert_true(r.success == true && r.timestamp == 1234567890);
    }

    HashTable__destroy(&me);
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(hash_table_test());
    return 0;
}
