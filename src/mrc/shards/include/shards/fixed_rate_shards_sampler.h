#pragma once
#ifdef __cplusplus
extern "C" {
#endif /*!__cplusplus*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "histogram/histogram.h"
#include "types/entry_type.h"

struct FixedRateShardsSampler {
    double sampling_ratio;
    uint64_t threshold;
    uint64_t scale;

    // SHARDS Adjustment Parameters
    bool adjustment;
    uint64_t num_entries_seen;
    uint64_t num_entries_processed;
};

bool
FixedRateShardsSampler__init(struct FixedRateShardsSampler *me,
                             double const sampling_ratio,
                             bool const adjustment);

void
FixedRateShardsSampler__destroy(struct FixedRateShardsSampler *me);

bool
FixedRateShardsSampler__sample(struct FixedRateShardsSampler *me,
                               EntryType entry);

void
FixedRateShardsSampler__post_process(struct FixedRateShardsSampler *me,
                                     struct Histogram *histogram);
bool
FixedRateShardsSampler__write_as_json(FILE *stream,
                                      struct FixedRateShardsSampler *me);

#ifdef __cplusplus
}
#endif /*!__cplusplus*/

// Here is some C++ only functionality.
#ifdef __cplusplus
#include <ostream>

static inline void
FixedRateShardsSampler__json(std::ostream &s,
                             struct FixedRateShardsSampler const &me,
                             bool const newline = true)
{
    s << "{";
    s << "\"type\": \"FixedRateShardsSampler\", ";
    s << "\".sampling_ratio\": " << me.sampling_ratio << ", ";
    s << "\".threshold\": " << me.threshold << ", ";
    s << "\".scale\": " << me.scale << ", ";
    s << "\".adjustment\": " << (me.adjustment ? "true" : "false") << ", ";
    s << "\".num_entries_seen\": " << me.num_entries_seen << ", ";
    s << "\".num_entries_processed\": " << me.num_entries_processed;
    s << "}";
    if (newline) {
        s << std::endl;
    }
}
#endif /*!__cplusplus*/
