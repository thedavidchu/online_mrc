#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "arrays/reverse_index.h"
#include "average_eviction_time/average_eviction_time.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "miss_rate_curve/miss_rate_curve.h"

bool
AverageEvictionTime__init(struct AverageEvictionTime *me,
                          const uint64_t histogram_num_bins,
                          const uint64_t histogram_bin_size)
{
    bool r = false;
    if (me == NULL || histogram_num_bins < 1 || histogram_bin_size < 1) {
        LOGGER_ERROR("invalid arguments");
        return false;
    }
    r = HashTable__init(&me->hash_table);
    if (!r) {
        LOGGER_ERROR("failed to init hash table");
        goto cleanup;
    }
    r = Histogram__init(&me->histogram, histogram_num_bins, histogram_bin_size);
    if (!r) {
        LOGGER_ERROR("failed to init histogram");
        goto cleanup;
    }
    me->current_time_stamp = 0;
    return true;
cleanup:
    HashTable__destroy(&me->hash_table);
    Histogram__destroy(&me->histogram);
    return false;
}

bool
AverageEvictionTime__access_item(struct AverageEvictionTime *me,
                                 EntryType entry)
{
    if (me == NULL)
        return false;
    struct LookupReturn s = HashTable__lookup(&me->hash_table, entry);
    if (s.success) {
        uint64_t const old_timestamp = s.timestamp;
        uint64_t const reuse_time = me->current_time_stamp - old_timestamp;
        if (!Histogram__insert_finite(&me->histogram, reuse_time))
            LOGGER_WARN("failed to insert into histogram");
        ++me->current_time_stamp;
    } else {
        enum PutUniqueStatus t = HashTable__put_unique(&me->hash_table,
                                                       entry,
                                                       me->current_time_stamp);
        if (t != LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE)
            LOGGER_WARN("failed to insert into hash table");
        if (!Histogram__insert_infinite(&me->histogram))
            LOGGER_WARN("failed to insert into histogram");
        ++me->current_time_stamp;
    }

    return true;
}

bool
AverageEvictionTime__post_process(struct AverageEvictionTime *me)
{
    // NOTE Do nothing here...
    if (me == NULL)
        return false;
    return true;
}

/// @brief  Convert the histogram to n * P(t)
static uint64_t *
get_n_times_prob(struct Histogram const *const me, size_t const num_bins)
{
    uint64_t *n_times_prob = calloc(num_bins + 1, sizeof(*n_times_prob));
    if (n_times_prob == NULL) {
        LOGGER_ERROR("could not allocate buffer");
        return NULL;
    }
#if 0
    // NOTE This version matches the description in the AET paper but it fails
    //      the invariant that the first bin in the MRC should have a miss rate
    //      of exactly 1.0. This is because it doesn't add the value from the
    //      first Probability.
    n_times_prob[num_bins] = me->infinity;
    n_times_prob[num_bins - 1] = n_times_prob[num_bins] + me->false_infinity;
    for (size_t i = 0; i < num_bins - 1; ++i) {
        size_t rev_i = REVERSE_INDEX(i, num_bins);
        n_times_prob[rev_i - 1] = n_times_prob[rev_i] + me->histogram[rev_i];
    }
#else
    n_times_prob[num_bins] = me->infinity + me->false_infinity;
    n_times_prob[num_bins - 1] =
        n_times_prob[num_bins] + me->histogram[num_bins - 1];
    for (size_t i = 0; i < num_bins - 1; ++i) {
        size_t rev_i = REVERSE_INDEX(i, num_bins);
        n_times_prob[rev_i - 1] =
            n_times_prob[rev_i] + me->histogram[rev_i - 1];
    }
#endif

    return n_times_prob;
}

static void
calculate_mrc(struct MissRateCurve *mrc,
              struct Histogram *me,
              size_t const num_bins,
              uint64_t const *const n_times_prob)
{
    assert(mrc && me && num_bins >= 1 && n_times_prob);
    uint64_t accum = 0;
    size_t current_cache_size = 0;
    for (size_t i = 0; i < num_bins; ++i) {
        accum += n_times_prob[i];
        if (accum >= current_cache_size * me->running_sum) {
            mrc->miss_rate[current_cache_size] =
                (double)n_times_prob[i] / me->running_sum;
            ++current_cache_size;
        }
    }

    // Set the rest of the MRC to the final value
    for (size_t i = current_cache_size; i < num_bins; ++i) {
        mrc->miss_rate[i] = mrc->miss_rate[current_cache_size - 1];
    }
}

bool
AverageEvictionTime__to_mrc(struct MissRateCurve *mrc, struct Histogram *me)
{
    if (mrc == NULL || me == NULL)
        return false;

    uint64_t const num_bins = me->num_bins;
    uint64_t const bin_size = me->bin_size;
    *mrc = (struct MissRateCurve){
        .miss_rate = calloc(num_bins + 1, sizeof(*mrc->miss_rate)),
        .num_bins = num_bins,
        .bin_size = bin_size,
    };

    if (mrc->miss_rate == NULL) {
        LOGGER_ERROR("calloc failed");
        return false;
    }

    uint64_t *n_times_prob = get_n_times_prob(me, num_bins);
    if (n_times_prob == NULL) {
        LOGGER_ERROR("could not allocate buffer");
        MissRateCurve__destroy(mrc);
        return NULL;
    }

    // Calculate MRC
    calculate_mrc(mrc, me, num_bins, n_times_prob);
    assert(MissRateCurve__validate(mrc));

    free(n_times_prob);
    return true;
}

void
AverageEvictionTime__destroy(struct AverageEvictionTime *me)
{
    if (me == NULL)
        return;
    HashTable__destroy(&me->hash_table);
    Histogram__destroy(&me->histogram);
    *me = (struct AverageEvictionTime){0};
    return;
}
