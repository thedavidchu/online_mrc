#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "math/positive_ceiling_divide.h"
#include "random/zipfian_random.h"
#include "test/mytester.h"

#include "bucketed_shards/bucketed_shards.h"
#include "mimir/mimir.h"
#include "olken/olken.h"
#include "parda_shards/parda_fixed_rate_shards.h"
#include "quickmrc/bucketed_quickmrc.h"
#include "quickmrc/quickmrc.h"
#include "shards/fixed_rate_shards.h"
#include "shards/fixed_size_shards.h"
#include "timer/timer.h"
#include "unused/mark_unused.h"

const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;
const double ZIPFIAN_RANDOM_SKEW = 0.5;
const uint64_t RANDOM_SEED = 0;
const uint64_t TRACE_LENGTH = 1 << 20;

#define PERFORMANCE_TEST(MRCStructType,                                        \
                         mrc_var_name,                                         \
                         init_expr,                                            \
                         access_item_func_name,                                \
                         destroy_func_name)                                    \
    do {                                                                       \
        struct ZipfianRandom zrng = {0};                                       \
        MRCStructType mrc_var_name = {0};                                      \
                                                                               \
        g_assert_true(ZipfianRandom__init(&zrng,                               \
                                          MAX_NUM_UNIQUE_ENTRIES,              \
                                          ZIPFIAN_RANDOM_SKEW,                 \
                                          RANDOM_SEED));                       \
        /* The maximum trace length is the number of possible unique items */  \
        g_assert_true(((init_expr)));                                          \
        double start_time = get_wall_time_sec();                               \
        for (uint64_t i = 0; i < TRACE_LENGTH; ++i) {                          \
            uint64_t key = ZipfianRandom__next(&zrng);                         \
            ((access_item_func_name))(&((mrc_var_name)), key);                 \
        }                                                                      \
        double end_time = get_wall_time_sec();                                 \
        double elapsed_time = (double)(end_time - start_time);                 \
        printf("Elapsed time for '" #MRCStructType "' workload: %.4f.\n",      \
               elapsed_time);                                                  \
        ZipfianRandom__destroy(&zrng);                                         \
        ((destroy_func_name))(&((mrc_var_name)));                              \
    } while (0)

static void
test_all(void)
{
    const uint64_t hist_bin_size = 1 << 10;
    const uint64_t hist_num_bins =
        POSITIVE_CEILING_DIVIDE(MAX_NUM_UNIQUE_ENTRIES, hist_bin_size);
    PERFORMANCE_TEST(struct Olken,
                     me,
                     Olken__init(&me, hist_num_bins, hist_bin_size),
                     Olken__access_item,
                     Olken__destroy);

    PERFORMANCE_TEST(
        struct FixedSizeShards,
        me,
        FixedSizeShards__init(&me, 1e-3, 1 << 13, hist_num_bins, hist_bin_size),
        FixedSizeShards__access_item,
        FixedSizeShards__destroy);

    PERFORMANCE_TEST(
        struct Mimir,
        me,
        Mimir__init(&me, 1000, hist_bin_size, hist_num_bins, MIMIR_ROUNDER),
        Mimir__access_item,
        Mimir__destroy);

#if 0
    PERFORMANCE_TEST(
        struct Mimir,
        me,
        Mimir__init(&me, 1000, hist_bin_size, hist_num_bins, MIMIR_STACKER),
        Mimir__access_item,
        Mimir__destroy);
#endif

    PERFORMANCE_TEST(struct PardaFixedRateShards,
                     me,
                     PardaFixedRateShards__init(&me, 1e-3),
                     PardaFixedRateShards__access_item,
                     PardaFixedRateShards__destroy);

    PERFORMANCE_TEST(
        struct QuickMRC,
        me,
        QuickMRC__init(&me, 1.0, 1024, 16, hist_num_bins, hist_bin_size),
        QuickMRC__access_item,
        QuickMRC__destroy);

    PERFORMANCE_TEST(
        struct FixedRateShards,
        me,
        FixedRateShards__init(&me, 1e-3, hist_num_bins, hist_bin_size, true),
        FixedRateShards__access_item,
        FixedRateShards__destroy);

    PERFORMANCE_TEST(
        struct BucketedShards,
        me,
        BucketedShards__init(&me, 1 << 13, hist_num_bins, 1e-3, hist_bin_size),
        BucketedShards__access_item,
        BucketedShards__destroy);
}

static void
test_sampling(void)
{
    const uint64_t hist_bin_size = 1 << 10;
    const uint64_t hist_num_bins =
        POSITIVE_CEILING_DIVIDE(MAX_NUM_UNIQUE_ENTRIES, hist_bin_size);

    UNUSED(hist_bin_size);
    UNUSED(hist_num_bins);
#if 1
    // Compare against Olken as a baseline
    PERFORMANCE_TEST(struct Olken,
                     me,
                     Olken__init(&me, hist_num_bins, hist_bin_size),
                     Olken__access_item,
                     Olken__destroy);
#endif
#if 1
    // Compare various SHARDS implementations
    // NOTE I order these simply by when I wrote it (from first to last)!
    PERFORMANCE_TEST(struct PardaFixedRateShards,
                     me,
                     PardaFixedRateShards__init(&me, 1e-3),
                     PardaFixedRateShards__access_item,
                     PardaFixedRateShards__destroy);
#endif

#if 1
    PERFORMANCE_TEST(
        struct FixedSizeShards,
        me,
        FixedSizeShards__init(&me, 1e-3, 1 << 13, hist_num_bins, hist_bin_size),
        FixedSizeShards__access_item,
        FixedSizeShards__destroy);
#endif

#if 1
    PERFORMANCE_TEST(
        struct FixedRateShards,
        me,
        FixedRateShards__init(&me, 1e-3, hist_num_bins, hist_bin_size, true),
        FixedRateShards__access_item,
        FixedRateShards__destroy);
#endif

#if 1
    PERFORMANCE_TEST(struct FixedRateShards,
                     me,
                     FixedRateShards__init(&me, 1e-12, 1, 1, true),
                     FixedRateShards__access_item,
                     FixedRateShards__destroy);
#endif

#if 1
    // Compare the novel SHARDS
    PERFORMANCE_TEST(
        struct BucketedShards,
        me,
        BucketedShards__init(&me, 1 << 13, hist_num_bins, 1e-3, hist_bin_size),
        BucketedShards__access_item,
        BucketedShards__destroy);
#endif
}

static void
test_quickmrc(void)
{
    const uint64_t hist_bin_size = 1 << 10;
    const uint64_t hist_num_bins =
        POSITIVE_CEILING_DIVIDE(MAX_NUM_UNIQUE_ENTRIES, hist_bin_size);

    UNUSED(hist_bin_size);
    UNUSED(hist_num_bins);
#if 1
    // Compare against Olken as a baseline
    PERFORMANCE_TEST(struct Olken,
                     me,
                     Olken__init(&me, hist_num_bins, hist_bin_size),
                     Olken__access_item,
                     Olken__destroy);
#endif
#if 1
    // Compare various SHARDS implementations
    PERFORMANCE_TEST(
        struct FixedSizeShards,
        me,
        FixedSizeShards__init(&me, 1e-3, 1 << 13, hist_num_bins, hist_bin_size),
        FixedSizeShards__access_item,
        FixedSizeShards__destroy);

    PERFORMANCE_TEST(
        struct FixedRateShards,
        me,
        FixedRateShards__init(&me, 1e-3, hist_num_bins, hist_bin_size, true),
        FixedRateShards__access_item,
        FixedRateShards__destroy);
#endif
    PERFORMANCE_TEST(
        struct QuickMRC,
        me,
        QuickMRC__init(&me, 1.0, 1024, 16, hist_num_bins, hist_bin_size),
        QuickMRC__access_item,
        QuickMRC__destroy);

    PERFORMANCE_TEST(
        struct QuickMRC,
        me,
        QuickMRC__init(&me, 1e-3, 1024, 16, hist_num_bins, hist_bin_size),
        QuickMRC__access_item,
        QuickMRC__destroy);

    PERFORMANCE_TEST(
        struct BucketedQuickMRC,
        me,
        BucketedQuickMRC__init(&me, 1024, 16, hist_num_bins, 1e-3, 1 << 13),
        BucketedQuickMRC__access_item,
        BucketedQuickMRC__destroy);
}

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    MAYBE_UNUSED(test_all);
    MAYBE_UNUSED(test_sampling);
    MAYBE_UNUSED(test_quickmrc);

#if 1
    test_all();
#endif
#if 1
    test_sampling();
#endif
#if 1
    test_quickmrc();
#endif

    return EXIT_SUCCESS;
}
