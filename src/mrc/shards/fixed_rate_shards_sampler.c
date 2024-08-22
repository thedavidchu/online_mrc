#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hash/hash.h"
#include "hash/types.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "math/ratio.h"
#include "shards/fixed_rate_shards_sampler.h"
#include "types/entry_type.h"

bool
FixedRateShardsSampler__init(struct FixedRateShardsSampler *me,
                             double const sampling_ratio,
                             bool const adjustment)
{

    if (me == NULL || sampling_ratio <= 0.0 || 1.0 < sampling_ratio)
        return false;
    me->sampling_ratio = sampling_ratio;
    me->threshold = ratio_uint64(sampling_ratio);
    me->scale = 1 / sampling_ratio;

    me->adjustment = adjustment;
    me->num_entries_seen = 0;
    me->num_entries_processed = 0;
    return true;
}

void
FixedRateShardsSampler__destroy(struct FixedRateShardsSampler *me)
{
    *me = (struct FixedRateShardsSampler){0};
}

bool
FixedRateShardsSampler__sample(struct FixedRateShardsSampler *me,
                               EntryType entry)
{
    ++me->num_entries_seen;
    Hash64BitType hash = Hash64Bit(entry);
    // NOTE Taking the modulo of the hash by 1 << 24 reduces the accuracy
    //      significantly. I tried dividing the threshold by 1 << 24 and also
    //      leaving the threshold alone. Neither worked to improve accuracy.
    if (hash > me->threshold)
        return false;
    ++me->num_entries_processed;
    return true;
}

void
FixedRateShardsSampler__post_process(struct FixedRateShardsSampler *me,
                                     struct Histogram *histogram)
{
    if (me == NULL || histogram == NULL || histogram->histogram == NULL ||
        histogram->num_bins < 1)
        return;

    if (!me->adjustment)
        return;

    // NOTE I need to scale the adjustment by the scale that I've been adjusting
    //      all values. Conversely, I could just not scale any values by the
    //      scale and I'd be equally well off (in fact, better probably,
    //      because a smaller chance of overflowing).
    const int64_t adjustment =
        me->scale *
        (me->num_entries_seen * me->sampling_ratio - me->num_entries_processed);
    bool r = Histogram__adjust_first_buckets(histogram, adjustment);
    if (!r) {
        LOGGER_WARN("error in adjusting buckets");
    }
}

bool
FixedRateShardsSampler__write_as_json(FILE *stream,
                                      struct FixedRateShardsSampler *me)
{
    if (stream == NULL) {
        LOGGER_ERROR("invalid stream");
        return false;
    }
    if (me == NULL) {
        fprintf(stream, "{\"type\": null}\n");
        return true;
    }
    fprintf(stream,
            "{\"type\": \"FixedRateShardsSampler\", \".sampling_ratio\": %g, "
            "\".threshold\": %" PRIu64 ", \".scale\": %" PRIu64
            ", \".adjustment\": %s, \".num_entries_seen\": %" PRIu64
            ", \".num_entries_processed\": %" PRIu64 "}\n",
            me->sampling_ratio,
            me->threshold,
            me->scale,
            me->adjustment ? "true" : "false",
            me->num_entries_seen,
            me->num_entries_processed);
    return true;
}
