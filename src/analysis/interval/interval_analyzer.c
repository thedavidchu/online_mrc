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

#include "file/file.h"
#include "interval/interval_olken.h"
#include "interval_statistics/interval_statistics.h"
#include "invariants/implies.h"
#include "logger/logger.h"
#include "shards/fixed_rate_shards_sampler.h"
#include "trace/reader.h"
#include "trace/trace.h"
#include "types/entry_type.h"

#ifndef INTERVAL_STATISTICS
#define INTERVAL_STATISTICS
#endif
#include "evicting_map/evicting_map.h"

struct CommandLineArguments {
    // Path to the input trace.
    char *input_path;
    // Input format.
    enum TraceFormat format;

    char *output_path;
    char *fr_shards_output_path;
    char *fs_shards_output_path;
    char *emap_output_path;

    double fr_shards_sampling_rate;
    double fs_shards_sampling_rate;

    bool cleanup;
};

static bool
parse_command_line_arguments(struct CommandLineArguments *const args,
                             int argc,
                             char *argv[])
{

    char *const DEFAULT_INPUT_PATH = NULL;
    enum TraceFormat const DEFAULT_INPUT_FORMAT = TRACE_FORMAT_KIA;

    char *const DEFAULT_OLKEN_OUTPUT_PATH = NULL;
    char *const DEFAULT_FIXED_RATE_SHARDS_OUTPUT_PATH = NULL;
    char *const DEFAULT_FIXED_SIZE_SHARDS_OUTPUT_PATH = NULL;
    char *const DEFAULT_EVICTING_MAP_OUTPUT_PATH = NULL;

    double const DEFAULT_FIXED_RATE_SHARDS_SAMPLING_RATE = 1e-1;
    double const DEFAULT_FIXED_SIZE_SHARDS_SAMPLING_RATE = 1e-3;

    bool const DEFAULT_CLEANUP_MODE = false;

    *args = (struct CommandLineArguments){
        .input_path = DEFAULT_INPUT_PATH,
        .format = DEFAULT_INPUT_FORMAT,
        .output_path = DEFAULT_OLKEN_OUTPUT_PATH,
        .fr_shards_output_path = DEFAULT_FIXED_RATE_SHARDS_OUTPUT_PATH,
        .fs_shards_output_path = DEFAULT_FIXED_SIZE_SHARDS_OUTPUT_PATH,
        .emap_output_path = DEFAULT_EVICTING_MAP_OUTPUT_PATH,
        .fr_shards_sampling_rate = DEFAULT_FIXED_RATE_SHARDS_SAMPLING_RATE,
        .fs_shards_sampling_rate = DEFAULT_FIXED_SIZE_SHARDS_SAMPLING_RATE,
        .cleanup = DEFAULT_CLEANUP_MODE,
    };

    char *trace_format_str = NULL;

    GOptionEntry entries[] = {
        {"input",
         'i',
         0,
         G_OPTION_ARG_FILENAME,
         &args->input_path,
         "input path to the trace",
         "<input-path>"},
        {"format",
         'f',
         0,
         G_OPTION_ARG_STRING,
         &trace_format_str,
         "format of the input file, either {Kia,Sari}. Default: Kia",
         "<input-format>"},
        {"output",
         'o',
         0,
         G_OPTION_ARG_FILENAME,
         &args->output_path,
         "Olken's output path to the interval-based histogram",
         "<olken-output-path>"},
        {"fr-output",
         'r',
         0,
         G_OPTION_ARG_FILENAME,
         &args->fr_shards_output_path,
         "fixed-rate SHARDS output path",
         "<fixed-rate-shards-output-path>"},
        {"fs-output",
         's',
         0,
         G_OPTION_ARG_FILENAME,
         &args->fs_shards_output_path,
         "fixed-size SHARDS output path",
         "<fixed-size-shards-output-path>"},
        {"evicting-map-output",
         'e',
         0,
         G_OPTION_ARG_FILENAME,
         &args->emap_output_path,
         "evicting map output path",
         "<evicting-map-output-path>"},
        {"fr-sampling-rate",
         0,
         0,
         G_OPTION_ARG_DOUBLE,
         &args->fr_shards_sampling_rate,
         "fixed-rate SHARDS sampling rate. Default: 1e-3.",
         "<fixed-rate-shards-sampling-rate>"},
        {"fs-sampling-rate",
         0,
         0,
         G_OPTION_ARG_DOUBLE,
         &args->fs_shards_sampling_rate,
         "fixed-size SHARDS sampling rate. Default: 1e-3.",
         "<fixed-size-shards-sampling-rate>"},
        {"cleanup",
         0,
         0,
         G_OPTION_ARG_NONE,
         &args->cleanup,
         "cleanup the generated files",
         NULL},
        G_OPTION_ENTRY_NULL,
    };

    GError *error = NULL;
    GOptionContext *context;
    context = g_option_context_new("- analyze MRC in intervals");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        exit(1);
    }

    bool is_error = false;
    if (!file_exists(args->input_path)) {
        LOGGER_ERROR("input path '%s' DNE", args->input_path);
        is_error = true;
    }
    if (trace_format_str != NULL &&
        !parse_trace_format_string(trace_format_str)) {
        LOGGER_ERROR("invalid trace format '%s'", trace_format_str);
        is_error = true;
    }
    if (is_error) {
        gchar *help_msg = g_option_context_get_help(context, FALSE, NULL);
        g_print("%s", help_msg);
        free(help_msg);
        exit(-1);
    }
    return true;
}

/// @brief  Create record of reuse distances and times.
bool
generate_olken_reuse_stats(struct Trace *trace,
                           struct CommandLineArguments const *const args)
{
    LOGGER_TRACE("starting generate_olken_reuse_stats(...)");
    if (trace == NULL || !implies(trace->length != 0, trace->trace != NULL)) {
        LOGGER_ERROR("invalid trace");
        return false;
    }

    struct IntervalOlken me = {0};
    struct FixedRateShardsSampler sampler = {0};
    if (!IntervalOlken__init(&me, trace->length) ||
        !FixedRateShardsSampler__init(&sampler,
                                      args->fr_shards_sampling_rate,
                                      true)) {
        LOGGER_ERROR("bad init");
        return false;
    }

    LOGGER_TRACE("beginning to process trace with length %zu", trace->length);
    for (size_t i = 0; i < trace->length; ++i) {
        EntryType const entry = trace->trace[i].key;
        if (FixedRateShardsSampler__sample(&sampler, entry))
            IntervalOlken__access_item(&me, entry);
    }

    size_t const length = sampler.num_entries_processed;

    LOGGER_TRACE("beginning to write buffer of length %zu to '%s'",
                 length,
                 args->output_path);
    if (!IntervalOlken__write_results(&me, args->output_path)) {
        LOGGER_ERROR("failed to write results to '%s'", args->output_path);
    }
    if (args->cleanup && remove(args->output_path) != 0) {
        LOGGER_ERROR("failed to remove '%s'", args->output_path);
    }
    LOGGER_TRACE("phew, finished writing the buffer!");
    IntervalOlken__destroy(&me);
    FixedRateShardsSampler__destroy(&sampler);
    return true;
}

bool
generate_emap_reuse_stats(struct Trace *trace,
                          struct CommandLineArguments const *const args)
{
    LOGGER_TRACE("starting generate_emap_reuse_stats(...)");
    if (trace == NULL || !implies(trace->length != 0, trace->trace != NULL)) {
        LOGGER_ERROR("invalid trace");
        return false;
    }

    struct EvictingMap emap = {0};
    if (!EvictingMap__init(&emap, 1e-1, 1 << 13, trace->length, 1)) {
        LOGGER_ERROR("bad init");
        return false;
    }

    LOGGER_TRACE("beginning to process trace with length %zu", trace->length);
    for (size_t i = 0; i < trace->length; ++i) {
        EntryType const entry = trace->trace[i].key;
        EvictingMap__access_item(&emap, entry);
    }

    // Save emap
    LOGGER_TRACE("write to '%s'", args->emap_output_path);
    IntervalStatistics__save(&emap.istats, args->emap_output_path);
    LOGGER_TRACE("phew, finished writing the buffer!");
    if (args->cleanup && remove(args->emap_output_path) != 0) {
        LOGGER_ERROR("failed to remove '%s'", args->emap_output_path);
    }
    EvictingMap__destroy(&emap);
    return true;
}

int
main(int argc, char *argv[])
{
    struct CommandLineArguments args = {0};
    if (!parse_command_line_arguments(&args, argc, argv)) {
        LOGGER_ERROR("unable to parse command line arguments");
        return EXIT_FAILURE;
    }
    enum TraceFormat format = args.format;
    if (format == TRACE_FORMAT_INVALID) {
        LOGGER_ERROR("invalid trace format");
        exit(-1);
    }
    LOGGER_TRACE("beginning to read trace file '%s' with format '%s'",
                 args.input_path,
                 TRACE_FORMAT_STRINGS[args.format]);
    struct Trace trace = read_trace(args.input_path, args.format);
    if (args.output_path)
        generate_olken_reuse_stats(&trace, &args);
    if (args.emap_output_path)
        generate_emap_reuse_stats(&trace, &args);

    return 0;
}
