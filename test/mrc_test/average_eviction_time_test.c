#include <stdbool.h>

#include "arrays/array_size.h"
#include "average_eviction_time/average_eviction_time.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "random/zipfian_random.h"
#include "test/mytester.h"

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
                                            histogram_oracle.bin_size));
    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        g_assert_true(AverageEvictionTime__access_item(&me, entries[i]));
    }
    g_assert_true(
        Histogram__debug_difference(&me.histogram, &histogram_oracle, 10));
    AverageEvictionTime__destroy(&me);
    return true;
}

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times());
    return 0;
}
