#pragma once

#include <bits/stdint-uintn.h>
#include <stdbool.h>
#include <stdint.h>

#include "histogram/histogram.h"
#include "lookup/hash_table.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "types/entry_type.h"

struct AverageEvictionTime {
    struct HashTable hash_table;
    struct Histogram histogram;
    uint64_t current_time_stamp;
};

bool
AverageEvictionTime__init(struct AverageEvictionTime *me,
                          const uint64_t histogram_num_bins,
                          const uint64_t histogram_bin_size);

bool
AverageEvictionTime__access_item(struct AverageEvictionTime *me,
                                 EntryType entry);

bool
AverageEvictionTime__post_process(struct AverageEvictionTime *me);

bool
AverageEvictionTime__to_mrc(struct MissRateCurve *mrc, struct Histogram *me);

void
AverageEvictionTime__destroy(struct AverageEvictionTime *me);
