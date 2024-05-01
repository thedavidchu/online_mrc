#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "random/zipfian_random.h"
#include "test/mytester.h"

#include "parda_shards/parda_fixed_rate_shards.h"
#include "shards/fixed_size_shards.h"
#include "mimir/buckets.h"
#include "mimir/mimir.h"
#include "olken/olken.h"
#include "quickmrc/quickmrc.h"
#include "unused/mark_unused.h"

const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;
const double ZIPFIAN_RANDOM_SKEW = 5.0e-1;
const uint64_t RANDOM_SEED = 0;

struct WorkerData {
    struct QuickMrc *qmrc;
    struct ZipfianRandom *zrng;
    pthread_barrier_t *barrier;
    EntryType *entries;
    uint64_t length;
};

static void *
parallel_thread_routine(void *args)
{
    struct WorkerData *data = args;
    const uint64_t insert_new_count = data->length * 20 / 100;
    const uint64_t reaccess_old_count = data->length - insert_new_count;
    for (uint64_t i = 0; i < insert_new_count; ++i) {
        quickmrc_buckets__insert_new(&data->qmrc->buckets);
    }
    pthread_barrier_wait(data->barrier);
    for (uint64_t i = 0; i < reaccess_old_count; ++i) {
        const TimeStampType timestamp = (TimeStampType)rand();
        quickmrc_buckets__reaccess_old(&data->qmrc->buckets, timestamp);
    }
    return NULL;
}

#define PERFORMANCE_TEST(MRCStructType,                                        \
                         mrc_var_name,                                         \
                         init_expr,                                            \
                         access_item_func_name,                                \
                         destroy_func_name)                                    \
    do {                                                                       \
        const uint64_t trace_length = 1 << 20;                                 \
        struct ZipfianRandom zrng = {0};                                       \
        MRCStructType mrc_var_name = {0};                                      \
                                                                               \
        g_assert_true(ZipfianRandom__init(&zrng,                              \
                                           MAX_NUM_UNIQUE_ENTRIES,             \
                                           ZIPFIAN_RANDOM_SKEW,                \
                                           RANDOM_SEED));                      \
        /* The maximum trace length is the number of possible unique items */  \
        g_assert_true(((init_expr)));                                          \
        clock_t start_time = clock();                                          \
        for (uint64_t i = 0; i < trace_length; ++i) {                          \
            uint64_t key = ZipfianRandom__next(&zrng);                        \
            ((access_item_func_name))(&((mrc_var_name)), key);                 \
        }                                                                      \
        clock_t end_time = clock();                                            \
        double elapsed_time =                                                  \
            (double)(end_time - start_time) / (double)CLOCKS_PER_SEC;          \
        printf("Elapsed time for '" #MRCStructType "' workload: %.4f.\n",      \
               elapsed_time);                                                  \
        ((destroy_func_name))(&((mrc_var_name)));                              \
        ZipfianRandom__destroy(&zrng);                                         \
    } while (0)

#define PERFORMANCE_TEST_PARALLEL(MRCStructType,                               \
                                  thread_count,                                \
                                  mrc_var_name,                                \
                                  init_expr,                                   \
                                  access_item_func_name,                       \
                                  destroy_func_name)                           \
    do {                                                                       \
        const uint64_t trace_length = 1 << 20;                                 \
        struct ZipfianRandom zrng = {0};                                       \
        MRCStructType mrc_var_name = {0};                                      \
                                                                               \
        g_assert_true(ZipfianRandom__init(&zrng,                              \
                                           MAX_NUM_UNIQUE_ENTRIES,             \
                                           ZIPFIAN_RANDOM_SKEW,                \
                                           RANDOM_SEED));                      \
        /* The maximum trace length is the number of possible unique items */  \
                                                                               \
        g_assert_true(((init_expr)));                                          \
        pthread_barrier_t barrier;                                             \
        pthread_barrier_init(&barrier, NULL, (thread_count));                  \
        const uint64_t accesses_per_thread = trace_length / (thread_count);    \
        const uint64_t accesses_remaining = trace_length % (thread_count);     \
        struct WorkerData data[thread_count];                                  \
        for (int i = 0; i < (thread_count); ++i) {                             \
            uint64_t access_count = accesses_per_thread;                       \
            if (i == 0) {                                                      \
                access_count += accesses_remaining;                            \
            }                                                                  \
                                                                               \
            data[i].qmrc = &me;                                                \
            data[i].zrng = &zrng;                                              \
            data[i].entries = NULL;                                            \
            data[i].length = access_count;                                     \
            data[i].barrier = &barrier;                                        \
        }                                                                      \
        pthread_t workers[(thread_count)] = {0};                               \
        clock_t start_time = clock();                                          \
        for (int i = 0; i < (thread_count); ++i) {                             \
            pthread_create(&workers[i],                                        \
                           NULL,                                               \
                           parallel_thread_routine,                            \
                           &data[i]);                                          \
        }                                                                      \
        for (int i = 0; i < (thread_count); ++i) {                             \
            pthread_join(workers[i], NULL);                                    \
        }                                                                      \
        clock_t end_time = clock();                                            \
        double elapsed_time =                                                  \
            (double)(end_time - start_time) / (double)CLOCKS_PER_SEC;          \
        printf("Elapsed time for '" #MRCStructType "' workload: %.4f.\n",      \
               elapsed_time);                                                  \
        ((destroy_func_name))(&((mrc_var_name)));                              \
        ZipfianRandom__destroy(&zrng);                                         \
        pthread_barrier_destroy(&barrier);                                     \
    } while (0)

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    PERFORMANCE_TEST(struct Olken,
                     me,
                     Olken__init(&me, MAX_NUM_UNIQUE_ENTRIES),
                     Olken__access_item,
                     Olken__destroy);

    PERFORMANCE_TEST_PARALLEL(struct QuickMrc,
                              1,
                              me,
                              quickmrc__init(&me, 60, 100, MAX_NUM_UNIQUE_ENTRIES),
                              quickmrc__access_item,
                              quickmrc__destroy);

    PERFORMANCE_TEST_PARALLEL(struct QuickMrc,
                              2,
                              me,
                              quickmrc__init(&me, 60, 100, MAX_NUM_UNIQUE_ENTRIES),
                              quickmrc__access_item,
                              quickmrc__destroy);

    PERFORMANCE_TEST_PARALLEL(struct QuickMrc,
                              4,
                              me,
                              quickmrc__init(&me, 60, 100, MAX_NUM_UNIQUE_ENTRIES),
                              quickmrc__access_item,
                              quickmrc__destroy);

    PERFORMANCE_TEST_PARALLEL(struct QuickMrc,
                              8,
                              me,
                              quickmrc__init(&me, 60, 100, MAX_NUM_UNIQUE_ENTRIES),
                              quickmrc__access_item,
                              quickmrc__destroy);

    PERFORMANCE_TEST_PARALLEL(struct QuickMrc,
                              16,
                              me,
                              quickmrc__init(&me, 60, 100, MAX_NUM_UNIQUE_ENTRIES),
                              quickmrc__access_item,
                              quickmrc__destroy);

    return EXIT_SUCCESS;
}
