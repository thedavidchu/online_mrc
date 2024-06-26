/** @brief  Generate a file of reuse distances and reuse times for an MRC.
 *
 *  This is useful because we can convert this stream into interval-based
 *  reuse distance histograms (for MRC generation). We can use the reuse time
 *  stream to find how many unique elements were accessed in a fixed interval.
 *
 *  An idea I would like to explore is whether we can find the number of unique
 *  accesses within an interval based only on the stream of stack distances.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "arrays/array_size.h"
#include "file/file.h"
#include "interval/interval_olken.h"
#include "invariants/implies.h"
#include "logger/logger.h"
#include "shards/fixed_rate_shards_sampler.h"
#include "trace/reader.h"
#include "trace/trace.h"
#include "unused/mark_unused.h"

#define IS_MAIN_FILE 1

#if IS_MAIN_FILE
static gchar *input_path = NULL;
static gchar *output_path = NULL;
static gchar *input_format = "Kia";
// NOTE By setting this to 1.0, we effectively shut off SHARDS.
static gdouble shards_sampling_rate = 1e0;
#endif

/// @brief  Create record of reuse distances and times.
bool
generate_reuse_stats(struct Trace *trace, char const *const fname)
{
    LOGGER_TRACE("starting generate_reuse_stats(%p, %s)", trace, fname);
    if (trace == NULL || !implies(trace->length != 0, trace->trace != NULL)) {
        LOGGER_ERROR("invalid trace");
        return false;
    }

    struct IntervalOlken me = {0};
    struct FixedRateShardsSampler sampler = {0};
    bool r = false;
    r = IntervalOlken__init(&me, trace->length);
    if (!r) {
        LOGGER_ERROR("bad init");
        return false;
    }
    r = FixedRateShardsSampler__init(&sampler, shards_sampling_rate, true);

    LOGGER_TRACE("beginning to process trace with length %zu", trace->length);
    for (size_t i = 0; i < trace->length; ++i) {
        if (FixedRateShardsSampler__sample(&sampler, trace->trace[i].key))
            IntervalOlken__access_item(&me, trace->trace[i].key);
    }

    size_t const length = sampler.num_entries_processed;

    LOGGER_TRACE("beginning to write buffer of length %zu to '%s'",
                 length,
                 fname);
    write_buffer(fname, me.stats, length, sizeof(*me.stats));
    LOGGER_TRACE("phew, finished writing the buffer!");
    IntervalOlken__destroy(&me);
    FixedRateShardsSampler__destroy(&sampler);
    return true;
}

#if IS_MAIN_FILE
static GOptionEntry entries[] = {
    {"input",
     'i',
     0,
     G_OPTION_ARG_FILENAME,
     &input_path,
     "input path to the trace",
     "<input-path>"},
    {"output",
     'o',
     0,
     G_OPTION_ARG_FILENAME,
     &output_path,
     "output path to the interval-based histogram",
     "<output-path>"},
    {"format",
     'f',
     0,
     G_OPTION_ARG_STRING,
     &input_format,
     "format of the input file, either {Kia,Sari}. Default: Kia",
     "<input-format>"},
    {"shards-sampling-rate",
     's',
     0,
     G_OPTION_ARG_DOUBLE,
     &shards_sampling_rate,
     "SHARDS sampling rate. Default: 1.0. [UNUSED]",
     "<rate>"},
    G_OPTION_ENTRY_NULL,
};

static bool
verify_path(gchar const *const path)
{
    return path != NULL;
}

int
main(int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *context;
    context = g_option_context_new("- analyze MRC in intervals");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        exit(1);
    }

    bool is_error = false;
    if (!verify_path(input_path)) {
        LOGGER_ERROR("invalid input path");
        is_error = true;
    }
    if (!verify_path(output_path)) {
        LOGGER_ERROR("invalid output path");
        is_error = true;
    }
    if (is_error) {
        gchar *help_msg = g_option_context_get_help(context, FALSE, NULL);
        g_print("%s", help_msg);
        free(help_msg);
        exit(-1);
    }

    enum TraceFormat format = parse_trace_format_string(input_format);
    if (format == TRACE_FORMAT_INVALID) {
        LOGGER_ERROR("invalid trace format");
        exit(-1);
    }
    LOGGER_TRACE("beginning to read trace file '%s' with format '%s'",
                 input_path,
                 input_format);
    struct Trace trace = read_trace(input_path, format);
    generate_reuse_stats(&trace, output_path);

    return 0;
}
#endif
