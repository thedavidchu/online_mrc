#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <glib.h>

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

static bool
single_thread_test(void)
{
    struct ParallelHashTable me = {0};
    g_assert_true(ParallelHashTable__init(&me, 8));

    for (size_t i = 0; i < N; ++i) {
        g_assert_true(ParallelHashTable__put(&me, i, i));
    }

    for (size_t i = 0; i < N; ++i) {
        struct LookupReturn r = ParallelHashTable__lookup(&me, i);
        g_assert_true(r.success == true && r.timestamp == i);
    }

    for (size_t i = 0; i < N; ++i) {
        g_assert_true(ParallelHashTable__put(&me, i, 1234567890));
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

static void *
multithread_writer(void *args)
{
    struct WorkerArgs *w = args;
    for (EntryType entry = w->start; entry < w->end; ++entry) {
        g_assert_true(ParallelHashTable__put(w->hash_table,
                                             entry,
                                             w->entry_to_timestamp(entry)));
    }
    return NULL;
}

static void *
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
static bool
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

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(single_thread_test());
    ASSERT_FUNCTION_RETURNS_TRUE(multi_thread_test());
    return 0;
}
