#pragma once
#include "types/time_stamp_type.h"
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "lookup/dictionary.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "priority_queue/heap.h"
#include "types/entry_type.h"

struct OlkenWithTTL {
    struct Olken olken;
    struct Heap pq;
    struct Dictionary const *dictionary;
};

bool
OlkenWithTTL__init(struct OlkenWithTTL *const me,
                   size_t const histogram_num_bins,
                   size_t const histogram_bin_size);

/// @brief  See 'OlkenWithTTL__init'.
/// @note   The interface is less stable than 'OlkenWithTTL__init'.
bool
OlkenWithTTL__init_full(struct OlkenWithTTL *const me,
                        size_t const histogram_num_bins,
                        size_t const histogram_bin_size,
                        enum HistogramOutOfBoundsMode const out_of_bounds_mode,
                        struct Dictionary const *const dictionary);

bool
OlkenWithTTL__access_item(struct OlkenWithTTL *const me,
                          EntryType const entry,
                          TimeStampType const timestamp_ms,
                          TimeStampType const ttl_s);

bool
OlkenWithTTL__post_process(struct OlkenWithTTL *me);

bool
OlkenWithTTL__to_mrc(struct OlkenWithTTL const *const me,
                     struct MissRateCurve *const mrc);

void
OlkenWithTTL__print_histogram_as_json(struct OlkenWithTTL *me);

void
OlkenWithTTL__destroy(struct OlkenWithTTL *me);

bool
OlkenWithTTL__get_histogram(struct OlkenWithTTL *const me,
                            struct Histogram const **const histogram);
