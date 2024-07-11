#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#include <glib.h>

#include "hash/MyMurmurHash3.h"
#include "lookup/evicting_hash_table.h"
#include "test/mytester.h"
#include "types/key_type.h"

#define HASH_FUNCTION(key) Hash64bit(key)
#define LENGTH             8
#define UNIQUE_KEYS        11

static bool
sampled_test(void)
{
    struct EvictingHashTable me = {0};
    uint64_t const first_val = 0, second_val = 1;

    g_assert_true(EvictingHashTable__init(&me, LENGTH, 1.0));

    for (size_t i = 0; i < UNIQUE_KEYS; ++i) {
        KeyType key = i;
        EvictingHashTable__put_unique(&me, key, first_val);
    }

    bool keys[UNIQUE_KEYS] = {0};
    unsigned num_keys = 0;

    for (size_t i = 0; i < UNIQUE_KEYS; ++i) {
        KeyType key = i;
        struct SampledLookupReturn s = EvictingHashTable__lookup(&me, key);
        if (s.status == SAMPLED_FOUND) {
            g_assert_cmpuint(s.hash, ==, HASH_FUNCTION(key));
            g_assert_cmpuint(s.timestamp, ==, first_val);
            keys[i] = true;
            ++num_keys;
        }
    }
    // We cannot have more found keys than there are slots in the hash table!
    g_assert_cmpuint(num_keys, <=, LENGTH);

    // Test update
    for (size_t i = 0; i < UNIQUE_KEYS; ++i) {
        KeyType key = i;
        struct SampledPutReturn r =
            EvictingHashTable__put_unique(&me, key, second_val);
        if (keys[i]) {
            g_assert_cmpuint(r.status, ==, SAMPLED_UPDATED);
            g_assert_cmpuint(r.new_hash, ==, HASH_FUNCTION(key));
            g_assert_cmpuint(r.old_timestamp, ==, 0);
            struct SampledLookupReturn s = EvictingHashTable__lookup(&me, key);
            g_assert_cmpuint(s.status, ==, SAMPLED_FOUND);
            g_assert_cmpuint(s.hash, ==, HASH_FUNCTION(key));
            g_assert_cmpuint(s.timestamp, ==, second_val);
        } else {
            g_assert_cmpuint(r.status, ==, SAMPLED_IGNORED);
        }
    }

    EvictingHashTable__destroy(&me);
    return true;
}

static bool
sampled_try_put_test(void)
{
    struct EvictingHashTable me = {0};
    uint64_t const first_val = 0, second_val = 1;

    g_assert_true(EvictingHashTable__init(&me, LENGTH, 1.0));

    for (size_t i = 0; i < UNIQUE_KEYS; ++i) {
        KeyType key = i;
        EvictingHashTable__try_put(&me, key, first_val);
    }

    bool keys[UNIQUE_KEYS] = {0};
    unsigned num_keys = 0;

    for (size_t i = 0; i < UNIQUE_KEYS; ++i) {
        KeyType key = i;
        struct SampledLookupReturn s = EvictingHashTable__lookup(&me, key);
        if (s.status == SAMPLED_FOUND) {
            g_assert_cmpuint(s.hash, ==, HASH_FUNCTION(key));
            g_assert_cmpuint(s.timestamp, ==, first_val);
            keys[i] = true;
            ++num_keys;
        }
    }
    // We cannot have more found keys than there are slots in the hash table!
    g_assert_cmpuint(num_keys, <=, LENGTH);

    // Test update
    for (size_t i = 0; i < UNIQUE_KEYS; ++i) {
        KeyType key = i;
        struct SampledTryPutReturn r =
            EvictingHashTable__try_put(&me, key, second_val);
        if (keys[i]) {
            g_assert_cmpuint(r.status, ==, SAMPLED_UPDATED);
            g_assert_cmpuint(r.new_hash, ==, HASH_FUNCTION(key));
            g_assert_cmpuint(r.old_key, ==, key);
            g_assert_cmpuint(r.old_hash, ==, HASH_FUNCTION(key));
            g_assert_cmpuint(r.old_value, ==, first_val);
            struct SampledLookupReturn s = EvictingHashTable__lookup(&me, key);
            g_assert_cmpuint(s.status, ==, SAMPLED_FOUND);
            g_assert_cmpuint(s.hash, ==, HASH_FUNCTION(key));
            g_assert_cmpuint(s.timestamp, ==, second_val);
        } else {
            g_assert_cmpuint(r.status, ==, SAMPLED_IGNORED);
        }
    }

    EvictingHashTable__destroy(&me);
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(sampled_test());
    ASSERT_FUNCTION_RETURNS_TRUE(sampled_try_put_test());
    return 0;
}
