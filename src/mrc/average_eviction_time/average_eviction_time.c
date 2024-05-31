#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "arrays/reverse_index.h"
#include "average_eviction_time/average_eviction_time.h"
#include "histogram/histogram.h"
#include "io/io.h"
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
        // We subtract an extra one so that the reuse_time between two
        // neighbouring accesses is 0.
        uint64_t const reuse_time = me->current_time_stamp - old_timestamp - 1;
        if (HashTable__put_unique(&me->hash_table,
                                  entry,
                                  me->current_time_stamp) !=
            LOOKUP_PUTUNIQUE_REPLACE_VALUE)
            LOGGER_WARN("failed to replace value in hash table");
        if (!Histogram__insert_finite(&me->histogram, reuse_time))
            LOGGER_WARN("failed to insert into histogram");
        ++me->current_time_stamp;
    } else {
        if (HashTable__put_unique(&me->hash_table,
                                  entry,
                                  me->current_time_stamp) !=
            LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE)
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

/// @brief  Convert the reuse time histogram to a scaled complement
///         cumulative distribution function.
/// @note   I 'scale' the reuse time CCDF to remain in the integer domain.
///         This is supposed to limit numeric errors.
static uint64_t *
get_scaled_rt_ccdf(struct Histogram const *const me, size_t const num_bins)
{
    uint64_t *rt_ccdf = calloc(num_bins + 1, sizeof(*rt_ccdf));
    if (rt_ccdf == NULL) {
        LOGGER_ERROR("could not allocate buffer");
        return NULL;
    }
#if 1
    // NOTE This version matches the description in the AET paper but it fails
    //      the invariant that the first bin in the MRC should have a miss rate
    //      of exactly 1.0. This is because it doesn't add the value from the
    //      first Probability. Additionally, it matches the oracle exactly for
    //      the step-function.
    rt_ccdf[num_bins] = me->infinity;
    rt_ccdf[num_bins - 1] = rt_ccdf[num_bins] + me->false_infinity;
    for (size_t i = 0; i < num_bins - 1; ++i) {
        size_t rev_i = REVERSE_INDEX(i, num_bins);
        rt_ccdf[rev_i - 1] = rt_ccdf[rev_i] + me->histogram[rev_i];
    }
#else
    rt_ccdf[num_bins] = me->infinity + me->false_infinity;
    rt_ccdf[num_bins - 1] = rt_ccdf[num_bins] + me->histogram[num_bins - 1];
    for (size_t i = 0; i < num_bins - 1; ++i) {
        size_t rev_i = REVERSE_INDEX(i, num_bins);
        rt_ccdf[rev_i - 1] = rt_ccdf[rev_i] + me->histogram[rev_i - 1];
    }
#endif
    return rt_ccdf;
}

static void
calculate_mrc(struct MissRateCurve *mrc,
              struct Histogram *me,
              size_t const num_bins,
              uint64_t const *const rt_ccdf)
{
    assert(mrc && me && num_bins >= 1 && rt_ccdf);
    uint64_t current_sum = 0;
    uint64_t const total = me->running_sum;
    size_t current_cache_size = 0;
    for (size_t i = 0; i < num_bins; ++i) {
        current_sum += rt_ccdf[i];
        if ((double)current_sum / total >= current_cache_size) {
            mrc->miss_rate[current_cache_size] = (double)rt_ccdf[i] / total;
            ++current_cache_size;
        }
    }

    // Set the rest of the MRC to the final value
    for (size_t i = current_cache_size; i < num_bins; ++i) {
        mrc->miss_rate[i] = mrc->miss_rate[current_cache_size - 1];
    }
}

bool
AverageEvictionTime__to_mrc(struct MissRateCurve *mrc, struct Histogram *hist)
{
    if (mrc == NULL || hist == NULL)
        return false;

    uint64_t const num_bins = hist->num_bins;
    uint64_t const bin_size = hist->bin_size;
    *mrc = (struct MissRateCurve){
        .miss_rate = calloc(num_bins + 1, sizeof(*mrc->miss_rate)),
        .num_bins = num_bins,
        .bin_size = bin_size,
    };

    if (mrc->miss_rate == NULL) {
        LOGGER_ERROR("calloc failed");
        return false;
    }

    uint64_t *rt_ccdf = get_scaled_rt_ccdf(hist, num_bins);
    if (rt_ccdf == NULL) {
        LOGGER_ERROR("could not allocate buffer");
        MissRateCurve__destroy(mrc);
        return NULL;
    }

    // Calculate MRC
    calculate_mrc(mrc, hist, num_bins, rt_ccdf);
    assert(MissRateCurve__validate(mrc));

    free(rt_ccdf);
    return true;
}

/// @brief  Calculate the Complement-Cumulative Distribution Function
///         from the reuse time histogram.
/// Source: https://dl-acm-org.myaccess.library.utoronto.ca/doi/10.1145/3185751
static double *
CalcCCDF(struct Histogram const *const rt)
{
    assert(rt);
    size_t const len = rt->num_bins;
    uint64_t const sum = rt->running_sum;

    assert(len > 0 && sum > 0);
    double *P = calloc(len, sizeof((*P)));
    assert(P);

    P[0] = 1.0;
    for (size_t i = 1; i < len; ++i) {
        P[i] = P[i - 1] - (double)rt->histogram[i] / sum;
    }
    return P;
}

static double *
CalcMRC(double const *const P, size_t const M, size_t const len)
{
    double integration = 0;
    size_t t = 0;

    assert(P && len > 0);
    double *MRC = calloc(M, sizeof((*P)));
    assert(MRC);

    // NOTE I set MRC[0] = 1.0 by definition; this was not present in
    //      the original pseudocode, which led to strange-looking MRCs.
    MRC[0] = 1.0;

    for (size_t c = 1; c < M; ++c) {
        while (integration < c && t <= len) {
            integration += P[t];
            ++t;
        }
        MRC[c] = P[t - 1];
    }
    return MRC;
}

bool
AverageEvictionTime__their_to_mrc(struct MissRateCurve *mrc,
                                  struct Histogram *hist)
{
    double *P = CalcCCDF(hist);
    double *MRC = CalcMRC(P, hist->num_bins, hist->num_bins);

    *mrc = (struct MissRateCurve){.miss_rate = MRC,
                                  .num_bins = hist->num_bins,
                                  .bin_size = hist->bin_size};

    free(P);
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
