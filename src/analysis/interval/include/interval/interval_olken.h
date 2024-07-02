#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "interval/interval_statistics.h"
#include "olken/olken.h"

struct IntervalOlken {
    struct Olken olken;
    struct ReuseStatistics stats;
};

bool
IntervalOlken__init(struct IntervalOlken *me, size_t const length);

bool
IntervalOlken__access_item(struct IntervalOlken *me, EntryType const entry);

bool
IntervalOlken__write_results(struct IntervalOlken *me, char const *const path);

void
IntervalOlken__destroy(struct IntervalOlken *me);
