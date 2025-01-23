#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "histogram/histogram.h"
#include "interval/interval_olken.h"
#include "interval_statistics/interval_statistics.h"
#include "invariants/implies.h"
#include "logger/logger.h"
#include "olken/olken.h"
#include "trace/reader.h"
#include "trace/trace.h"
#include "types/entry_type.h"

bool
test_interval_olken(struct Trace *trace)
{
    if (trace == NULL || !implies(trace->length != 0, trace->trace != NULL)) {
        LOGGER_ERROR("invalid trace");
        return false;
    }

    struct Olken olken = {0};
    struct IntervalOlken me = {0};

    size_t const hist_num_bins = trace->length;
    size_t const hist_bin_size = 1;

    if (!Olken__init(&olken, hist_num_bins, hist_bin_size)) {
        LOGGER_ERROR("failed to init Olken");
        return false;
    }
    if (!IntervalOlken__init(&me, trace->length)) {
        LOGGER_ERROR("failed to init interval Olken");
        return false;
    }
    for (size_t i = 0; i < trace->length; ++i) {
        EntryType entry = trace->trace[i].key;
        Olken__access_item(&olken, entry);
        IntervalOlken__access_item(&me, entry);
    }

    struct Histogram hist = {0};
    if (!IntervalStatistics__to_histogram(&me.stats,
                                          &hist,
                                          hist_num_bins,
                                          hist_bin_size)) {
        LOGGER_ERROR("failed to convert to histogram");
        return false;
    }

    // Test for equality
    if (!Histogram__exactly_equal(&olken.histogram, &hist)) {
        LOGGER_ERROR("histograms not equal");
        return false;
    }

    Olken__destroy(&olken);
    IntervalOlken__destroy(&me);
    return true;
}

int
main(int argc, char *argv[])
{
    assert(argc == 2);
    char const *const input_path = argv[1];

    struct Trace trace = read_trace_keys(input_path, TRACE_FORMAT_KIA);
    g_assert_true(test_interval_olken(&trace));

    return 0;
}
