#include <stdbool.h>
#include <stdint.h>

#include "arrays/array_size.h"
#include "average_eviction_time/average_eviction_time.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "random/zipfian_random.h"
#include "test/mytester.h"
#include "trace/generator.h"
#include "trace/trace.h"

static bool
access_same_key_five_times(void)
{
    EntryType entries[5] = {0, 0, 0, 0, 0};
    uint64_t histogram_oracle_array[11] = {4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    struct Histogram histogram_oracle = {
        .histogram = histogram_oracle_array,
        .num_bins = ARRAY_SIZE(histogram_oracle_array),
        .bin_size = 1,
        .false_infinity = 0,
        .infinity = 1,
        .running_sum = ARRAY_SIZE(entries),
    };

    struct AverageEvictionTime me = {0};
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(AverageEvictionTime__init(&me,
                                            histogram_oracle.num_bins,
                                            histogram_oracle.bin_size,
                                            false));
    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        g_assert_true(AverageEvictionTime__access_item(&me, entries[i]));
    }
    g_assert_true(
        Histogram__debug_difference(&me.histogram, &histogram_oracle, 10));
    AverageEvictionTime__destroy(&me);
    return true;
}

static bool
test_on_step_trace(void)
{
    struct Trace trace = generate_step_trace(100, 10);
    if (trace.trace == NULL || trace.length != 100) {
        LOGGER_ERROR("bad trace");
        return false;
    }

    struct AverageEvictionTime me = {0};
    struct Olken oracle = {0};
    g_assert_true(AverageEvictionTime__init(&me, 10, 1, false));
    g_assert_true(Olken__init(&oracle, 10, 1));

    for (size_t i = 0; i < trace.length; ++i) {
        uint64_t key = trace.trace[i].key;
        g_assert_true(AverageEvictionTime__access_item(&me, key));
        g_assert_true(Olken__access_item(&oracle, key));
    }

    g_assert_true(
        Histogram__debug_difference(&me.histogram, &oracle.histogram, 10));

    // Check the MAE exactly matches
    struct MissRateCurve mrc = {0}, oracle_mrc = {0};
    g_assert_true(MissRateCurve__init_from_histogram(&mrc, &me.histogram));
    g_assert_true(
        MissRateCurve__init_from_histogram(&oracle_mrc, &oracle.histogram));
    double const mae = MissRateCurve__mean_absolute_error(&mrc, &oracle_mrc);
    g_assert_cmpfloat(mae, ==, 0.0);

    AverageEvictionTime__destroy(&me);
    Olken__destroy(&oracle);
    return true;
}

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times());
    ASSERT_FUNCTION_RETURNS_TRUE(test_on_step_trace());
    return 0;
}
