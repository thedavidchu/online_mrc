#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "random/zipfian_random.h"
#include "test/mytester.h"

#include "fixed_size_shards/fixed_size_shards.h"
#include "mimir/buckets.h"
#include "mimir/mimir.h"
#include "olken/olken.h"
#include "unused/mark_unused.h"

const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;
const double ZIPFIAN_RANDOM_SKEW = 5.0e-1;
const uint64_t RANDOM_SEED = 0;

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
        ASSERT_TRUE_WITHOUT_CLEANUP(                                           \
            zipfian_random__init(&zrng,                                        \
                                 MAX_NUM_UNIQUE_ENTRIES,                       \
                                 ZIPFIAN_RANDOM_SKEW,                          \
                                 RANDOM_SEED));                                \
        /* The maximum trace length is the number of possible unique items */  \
        ASSERT_TRUE_WITHOUT_CLEANUP(((init_expr)));                            \
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

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    PERFORMANCE_TEST(struct OlkenReuseStack,
                     me,
                     olken__init(&me, MAX_NUM_UNIQUE_ENTRIES),
                     olken__access_item,
                     olken__destroy);

    PERFORMANCE_TEST(
        struct FixedSizeShardsReuseStack,
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

    PERFORMANCE_TEST(
        struct Mimir,
        me,
        mimir__init(&me, 1000, MAX_NUM_UNIQUE_ENTRIES, MIMIR_STACKER),
        mimir__access_item,
        mimir__destroy);

    return EXIT_SUCCESS;
}
