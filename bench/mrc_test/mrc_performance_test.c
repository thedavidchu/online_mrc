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
    EntryType *entries;
    uint64_t length;
};

static void *
parallel_thread_routine(void *args)
{
    struct WorkerData *data = args;
    for (uint64_t i = 0; i < data->length; ++i) {
        uint64_t key = zipfian_random__next(data->zrng);
        quickmrc__access_item(data->qmrc, key);
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
        g_assert_true(zipfian_random__init(&zrng,                              \
                                           MAX_NUM_UNIQUE_ENTRIES,             \
                                           ZIPFIAN_RANDOM_SKEW,                \
                                           RANDOM_SEED));                      \
        /* The maximum trace length is the number of possible unique items */  \
        g_assert_true(((init_expr)));                                          \
        clock_t start_time = clock();                                          \
        for (uint64_t i = 0; i < trace_length; ++i) {                          \
            uint64_t key = zipfian_random__next(&zrng);                        \
            ((access_item_func_name))(&((mrc_var_name)), key);                 \
        }                                                                      \
        clock_t end_time = clock();                                            \
        double elapsed_time =                                                  \
            (double)(end_time - start_time) / (double)CLOCKS_PER_SEC;          \
        printf("Elapsed time for '" #MRCStructType "' workload: %.4f.\n",      \
               elapsed_time);                                                  \
        ((destroy_func_name))(&((mrc_var_name)));                              \
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
        g_assert_true(zipfian_random__init(&zrng,                              \
                                           MAX_NUM_UNIQUE_ENTRIES,             \
                                           ZIPFIAN_RANDOM_SKEW,                \
                                           RANDOM_SEED));                      \
        /* The maximum trace length is the number of possible unique items */  \
                                                                               \
        g_assert_true(((init_expr)));                                          \
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

    PERFORMANCE_TEST(
        struct FixedSizeShards,
        me,
        fixed_size_shards__init(&me, 1000, 10000, MAX_NUM_UNIQUE_ENTRIES),
        fixed_size_shards__access_item,
        fixed_size_shards__destroy);

    PERFORMANCE_TEST(
        struct Mimir,
        me,
        mimir__init(&me, 1000, MAX_NUM_UNIQUE_ENTRIES, MIMIR_ROUNDER),
        mimir__access_item,
        mimir__destroy);

    bool i_have_lots_of_spare_cpu_cycles = false;
    if (i_have_lots_of_spare_cpu_cycles) {
        PERFORMANCE_TEST(
            struct Mimir,
            me,
            mimir__init(&me, 1000, MAX_NUM_UNIQUE_ENTRIES, MIMIR_STACKER),
            mimir__access_item,
            mimir__destroy);
    }

    PERFORMANCE_TEST(struct PardaFixedRateShards,
                     me,
                     PardaFixedRateShards__init(&me, 1e-3),
                     PardaFixedRateShards__access_item,
                     PardaFixedRateShards__destroy);

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
