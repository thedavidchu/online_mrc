#include <glib.h>
#include <stdbool.h>
#include <stddef.h>

#include <pthread.h>
#include <stdio.h>

#include "hash/splitmix64.h"
#include "logger/logger.h"
#include "lookup/evicting_hash_table.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "lookup/parallel_hash_table.h"
#include "test/mytester.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "unused/mark_unused.h"

#define N 1000

static TimeStampType
identity(EntryType entry)
{
    return entry;
}

static TimeStampType
constant_1234567890(EntryType entry)
{
    UNUSED(entry);
    return 1234567890;
}

////////////////////////////////////////////////////////////////////////////////
/// SERIAL HASH TABLE TEST
////////////////////////////////////////////////////////////////////////////////
bool
hash_table_test(void)
{
    struct HashTable me = {0};
    g_assert_true(HashTable__init(&me));

    for (size_t i = 0; i < N; ++i) {
        g_assert_true(HashTable__put_unique(&me, i, i));
    }

    for (size_t i = 0; i < N; ++i) {
        struct LookupReturn r = HashTable__lookup(&me, i);
        g_assert_true(r.success == true && r.timestamp == i);
    }

    for (size_t i = 0; i < N; ++i) {
        g_assert_true(HashTable__put_unique(&me, i, 1234567890));
    }

    for (size_t i = 0; i < N; ++i) {
        struct LookupReturn r = HashTable__lookup(&me, i);
        g_assert_true(r.success == true && r.timestamp == 1234567890);
    }

    HashTable__destroy(&me);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
/// PARALLEL HASH TABLE TEST
////////////////////////////////////////////////////////////////////////////////

bool
single_thread_test(void)
{
    struct ParallelHashTable me = {0};
    g_assert_true(ParallelHashTable__init(&me, 8));

    for (size_t i = 0; i < N; ++i) {
        g_assert_true(ParallelHashTable__put_unique(&me, i, i));
    }

    for (size_t i = 0; i < N; ++i) {
        struct LookupReturn r = ParallelHashTable__lookup(&me, i);
        g_assert_true(r.success == true && r.timestamp == i);
    }

    for (size_t i = 0; i < N; ++i) {
        g_assert_true(ParallelHashTable__put_unique(&me, i, 1234567890));
    }

    for (size_t i = 0; i < N; ++i) {
        struct LookupReturn r = ParallelHashTable__lookup(&me, i);
        g_assert_true(r.success == true && r.timestamp == 1234567890);
    }

    ParallelHashTable__destroy(&me);
    return true;
}

struct WorkerArgs {
    int worker_id;
    struct ParallelHashTable *hash_table;
    TimeStampType (*entry_to_timestamp)(EntryType entry);
    EntryType start, end;
};

void *
multithread_writer(void *args)
{
    struct WorkerArgs *w = args;
    for (EntryType entry = w->start; entry < w->end; ++entry) {
        g_assert_true(
            ParallelHashTable__put_unique(w->hash_table,
                                          entry,
                                          w->entry_to_timestamp(entry)));
    }
    return NULL;
}

void *
multithread_reader(void *args)
{
    struct WorkerArgs *w = args;
    for (EntryType entry = w->start; entry < w->end; ++entry) {
        struct LookupReturn r = ParallelHashTable__lookup(w->hash_table, entry);
        g_assert_true(r.success == true);
        g_assert_true(r.timestamp == w->entry_to_timestamp(entry));
    }
    return NULL;
}

/// @brief  Test the multithread hash table with some overlaps.
bool
multi_thread_test(void)
{
    struct ParallelHashTable me = {0};
    g_assert_true(ParallelHashTable__init(&me, 8));

    pthread_t threads[16] = {0};
    struct WorkerArgs args[16] = {0};

    // Write the values
    for (size_t i = 0; i < 16; ++i) {
        args[i] = (struct WorkerArgs){.worker_id = i,
                                      .start = i * N,
                                      .end = (i + 2) * N,
                                      .hash_table = &me,
                                      .entry_to_timestamp = identity};
        pthread_create(&threads[i], NULL, multithread_writer, &args[i]);
    }
    for (size_t i = 0; i < 16; ++i) {
        pthread_join(threads[i], NULL);
    }

    // Read the values
    for (size_t i = 0; i < 16; ++i) {
        args[i] = (struct WorkerArgs){.worker_id = i,
                                      .start = i * N,
                                      .end = (i + 2) * N,
                                      .hash_table = &me,
                                      .entry_to_timestamp = identity};
        pthread_create(&threads[i], NULL, multithread_reader, &args[i]);
    }
    for (size_t i = 0; i < 16; ++i) {
        pthread_join(threads[i], NULL);
    }

    // Write the values
    for (size_t i = 0; i < 16; ++i) {
        args[i] =
            (struct WorkerArgs){.worker_id = i,
                                .start = i * N,
                                .end = (i + 2) * N,
                                .hash_table = &me,
                                .entry_to_timestamp = constant_1234567890};
        pthread_create(&threads[i], NULL, multithread_writer, &args[i]);
    }
    for (size_t i = 0; i < 16; ++i) {
        pthread_join(threads[i], NULL);
    }

    // Read the values
    for (size_t i = 0; i < 16; ++i) {
        args[i] =
            (struct WorkerArgs){.worker_id = i,
                                .start = i * N,
                                .end = (i + 2) * N,
                                .hash_table = &me,
                                .entry_to_timestamp = constant_1234567890};
        pthread_create(&threads[i], NULL, multithread_reader, &args[i]);
    }
    for (size_t i = 0; i < 16; ++i) {
        pthread_join(threads[i], NULL);
    }
    ParallelHashTable__destroy(&me);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
/// SAMPLED HASH TABLE TEST
////////////////////////////////////////////////////////////////////////////////
static void
sampled_insert(struct EvictingHashTable *me,
               uint64_t key,
               uint64_t value,
               enum SampledStatus expected_put_status)
{
    assert(me != NULL);

    // Insert with an expected status
    const bool debug = false;
    if (debug) {
        LOGGER_DEBUG("key: %lu, value: %lu, expected_put_status: %d",
                     key,
                     value,
                     expected_put_status);
        EvictingHashTable__print_as_json(me);
    }
    g_assert_cmpint(EvictingHashTable__put_unique(me, key, value).status,
                    ==,
                    expected_put_status);

    // Lookup with expected status
    switch (expected_put_status) {
    case SAMPLED_IGNORED: {
        struct SampledLookupReturn r = EvictingHashTable__lookup(me, key);
        g_assert_cmpuint(r.status, ==, SAMPLED_IGNORED);
        break;
    }
    case SAMPLED_INSERTED: /* Intentional fall through... */
    case SAMPLED_UPDATED:  /* Intentional fall through... */
    case SAMPLED_REPLACED: {
        struct SampledLookupReturn r = EvictingHashTable__lookup(me, key);
        g_assert_cmpuint(r.status, ==, SAMPLED_FOUND);
        g_assert_cmpuint(r.hash, ==, splitmix64_hash(key));
        g_assert_cmpuint(r.timestamp, ==, value);
        break;
    }
    case SAMPLED_FOUND:    /* Intentional fall through... */
    case SAMPLED_NOTFOUND: /* Intentional fall through... */
    default:
        assert(0 && "impossible");
    }
}

bool
sampled_test(void)
{
    struct EvictingHashTable me = {0};
    g_assert_true(EvictingHashTable__init(&me, 8, 1.0));

    // Test inserts
    sampled_insert(&me, 0, 0, SAMPLED_INSERTED);
    sampled_insert(&me, 1, 0, SAMPLED_INSERTED);
    sampled_insert(&me, 2, 0, SAMPLED_INSERTED);
    sampled_insert(&me, 3, 0, SAMPLED_IGNORED);
    sampled_insert(&me, 4, 0, SAMPLED_INSERTED);
    sampled_insert(&me, 5, 0, SAMPLED_REPLACED);
    sampled_insert(&me, 6, 0, SAMPLED_IGNORED);
    sampled_insert(&me, 7, 0, SAMPLED_REPLACED);
    sampled_insert(&me, 8, 0, SAMPLED_IGNORED);
    sampled_insert(&me, 9, 0, SAMPLED_INSERTED);
    sampled_insert(&me, 10, 0, SAMPLED_INSERTED);

    // Test updates
    sampled_insert(&me, 0, 1, SAMPLED_UPDATED);
    sampled_insert(&me, 1, 1, SAMPLED_UPDATED);
    sampled_insert(&me, 2, 1, SAMPLED_UPDATED);
    sampled_insert(&me, 3, 1, SAMPLED_IGNORED);
    sampled_insert(&me, 4, 1, SAMPLED_IGNORED);
    sampled_insert(&me, 5, 1, SAMPLED_IGNORED);
    sampled_insert(&me, 6, 1, SAMPLED_IGNORED);
    sampled_insert(&me, 7, 1, SAMPLED_UPDATED);
    sampled_insert(&me, 8, 1, SAMPLED_IGNORED);
    sampled_insert(&me, 9, 1, SAMPLED_UPDATED);
    sampled_insert(&me, 10, 1, SAMPLED_UPDATED);

    EvictingHashTable__destroy(&me);
    return true;
}

static void
sampled_try_put(struct EvictingHashTable *me,
                uint64_t key,
                uint64_t value,
                enum SampledStatus expected_put_status)
{
    assert(me != NULL);

    // Insert with an expected status
    const bool debug = true;
    if (debug) {
        LOGGER_DEBUG("key: %lu, value: %lu, expected_put_status: %d",
                     key,
                     value,
                     expected_put_status);
        EvictingHashTable__print_as_json(me);
    }
    g_assert_cmpint(EvictingHashTable__try_put(me, key, value).status,
                    ==,
                    expected_put_status);

    // Lookup with expected status
    switch (expected_put_status) {
    case SAMPLED_IGNORED: {
        struct SampledLookupReturn r = EvictingHashTable__lookup(me, key);
        g_assert_cmpuint(r.status, ==, SAMPLED_IGNORED);
        break;
    }
    case SAMPLED_INSERTED: /* Intentional fall through... */
    case SAMPLED_UPDATED:  /* Intentional fall through... */
    case SAMPLED_REPLACED: {
        struct SampledLookupReturn r = EvictingHashTable__lookup(me, key);
        g_assert_cmpuint(r.status, ==, SAMPLED_FOUND);
        g_assert_cmpuint(r.hash, ==, splitmix64_hash(key));
        g_assert_cmpuint(r.timestamp, ==, value);
        break;
    }
    case SAMPLED_FOUND:    /* Intentional fall through... */
    case SAMPLED_NOTFOUND: /* Intentional fall through... */
    default:
        assert(0 && "impossible");
    }
}

static bool
sampled_try_put_test(void)
{
    struct EvictingHashTable me = {0};
    g_assert_true(EvictingHashTable__init(&me, 8, 1.0));

    // Test inserts
    sampled_try_put(&me, 0, 0, SAMPLED_INSERTED);
    sampled_try_put(&me, 1, 0, SAMPLED_INSERTED);
    sampled_try_put(&me, 2, 0, SAMPLED_INSERTED);
    sampled_try_put(&me, 3, 0, SAMPLED_IGNORED);
    sampled_try_put(&me, 4, 0, SAMPLED_INSERTED);
    sampled_try_put(&me, 5, 0, SAMPLED_REPLACED);
    sampled_try_put(&me, 6, 0, SAMPLED_IGNORED);
    sampled_try_put(&me, 7, 0, SAMPLED_REPLACED);
    sampled_try_put(&me, 8, 0, SAMPLED_IGNORED);
    sampled_try_put(&me, 9, 0, SAMPLED_INSERTED);
    sampled_try_put(&me, 10, 0, SAMPLED_INSERTED);

    // Test updates
    sampled_try_put(&me, 0, 1, SAMPLED_UPDATED);
    sampled_try_put(&me, 1, 1, SAMPLED_UPDATED);
    sampled_try_put(&me, 2, 1, SAMPLED_UPDATED);
    sampled_try_put(&me, 3, 1, SAMPLED_IGNORED);
    sampled_try_put(&me, 4, 1, SAMPLED_IGNORED);
    sampled_try_put(&me, 5, 1, SAMPLED_IGNORED);
    sampled_try_put(&me, 6, 1, SAMPLED_IGNORED);
    sampled_try_put(&me, 7, 1, SAMPLED_UPDATED);
    sampled_try_put(&me, 8, 1, SAMPLED_IGNORED);
    sampled_try_put(&me, 9, 1, SAMPLED_UPDATED);
    sampled_try_put(&me, 10, 1, SAMPLED_UPDATED);

    EvictingHashTable__destroy(&me);
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(hash_table_test());
    ASSERT_FUNCTION_RETURNS_TRUE(single_thread_test());
    ASSERT_FUNCTION_RETURNS_TRUE(multi_thread_test());
    ASSERT_FUNCTION_RETURNS_TRUE(sampled_test());
    ASSERT_FUNCTION_RETURNS_TRUE(sampled_try_put_test());
    return 0;
}
