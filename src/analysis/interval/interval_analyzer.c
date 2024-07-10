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
#include "trace/generator.h"
#include "trace/reader.h"
#include "trace/trace.h"
#include "types/entry_type.h"

#ifndef INTERVAL_STATISTICS
#define INTERVAL_STATISTICS
#endif
#include "evicting_map/evicting_map.h"
#include "shards/fixed_rate_shards.h"
#include "shards/fixed_size_shards.h"

struct CommandLineArguments {
    // Path to the input trace.
    char *input_path;
    // Input format.
    enum TraceFormat format;
    gboolean run_uniform;
    gboolean run_zipfian;

    char *output_path;
    char *fr_shards_output_path;
    char *fs_shards_output_path;
    char *emap_output_path;

    double fr_shards_sampling_rate;
    double fs_shards_sampling_rate;

    gboolean cleanup;
};

static bool
parse_command_line_arguments(struct CommandLineArguments *const args,
                             int argc,
                             char *argv[])
{

    char *const DEFAULT_INPUT_PATH = NULL;
    enum TraceFormat const DEFAULT_INPUT_FORMAT = TRACE_FORMAT_KIA;
    bool DEFAULT_RUN_UNIFORM = FALSE;
    bool DEFAULT_RUN_ZIPFIAN = FALSE;

    char *const DEFAULT_OLKEN_OUTPUT_PATH = NULL;
    char *const DEFAULT_FIXED_RATE_SHARDS_OUTPUT_PATH = NULL;
    char *const DEFAULT_FIXED_SIZE_SHARDS_OUTPUT_PATH = NULL;
    char *const DEFAULT_EVICTING_MAP_OUTPUT_PATH = NULL;

    double const DEFAULT_FIXED_RATE_SHARDS_SAMPLING_RATE = 1e-3;
    double const DEFAULT_FIXED_SIZE_SHARDS_SAMPLING_RATE = 1e-1;

    bool const DEFAULT_CLEANUP_MODE = FALSE;

    *args = (struct CommandLineArguments){
        .input_path = DEFAULT_INPUT_PATH,
        .format = DEFAULT_INPUT_FORMAT,
        .run_uniform = DEFAULT_RUN_UNIFORM,
        .run_zipfian = DEFAULT_RUN_ZIPFIAN,
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
        {"uniform",
         'u',
         0,
         G_OPTION_ARG_NONE,
         &args->run_uniform,
         "run uniform random trace",
         0},
        {"zipfian",
         'z',
         0,
         G_OPTION_ARG_NONE,
         &args->run_zipfian,
         "run zipfian random trace",
         0},
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
         "fixed-size SHARDS sampling rate. Default: 1e-1.",
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
    if (args->input_path != NULL && !file_exists(args->input_path)) {
        LOGGER_ERROR("input path '%s' DNE", args->input_path);
        is_error = true;
    }
    if (args->input_path == NULL && !args->run_uniform && !args->run_zipfian) {
        LOGGER_ERROR("no input selected!");
        is_error = true;
    }
    if ((args->input_path != NULL && args->run_uniform) ||
        (args->input_path != NULL && args->run_zipfian) ||
        (args->run_uniform && args->run_zipfian)) {
        LOGGER_WARN("selected multiple inputs, so we'll end up overwriting "
                    "some (Zipfian overwrites Uniform which overwrites the "
                    "trace): (-i:%p,-u:%d,-z:%d)",
                    args->input_path,
                    args->run_uniform,
                    args->run_zipfian);
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

    g_option_context_free(context);
    return true;
}

/// @param  cleanup: bool
///         immediately remove the file we just created. Alternatively,
///         I could just not save it in the first place, but I want to
///         test everything.
static void
save_interval_statistics(struct IntervalStatistics const *const istats,
                         char const *const output_path,
                         bool const cleanup)
{
    LOGGER_TRACE("begin writing buffer with length %zu", istats->length);
    if (!IntervalStatistics__save(istats, output_path)) {
        LOGGER_ERROR("failed to write results to '%s'", output_path);
    }
    if (cleanup && remove(output_path) != 0) {
        LOGGER_ERROR("failed to remove '%s'", output_path);
    }
    LOGGER_TRACE("finish writing buffer");
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
    if (args == NULL) {
        LOGGER_ERROR("bad args");
        return false;
    }

    struct IntervalOlken me = {0};
    if (!IntervalOlken__init(&me, trace->length)) {
        LOGGER_ERROR("bad initialization");
        return false;
    }

    LOGGER_TRACE("begin processing trace with length %zu", trace->length);
    for (size_t i = 0; i < trace->length; ++i) {
        EntryType const entry = trace->trace[i].key;
        IntervalOlken__access_item(&me, entry);
    }
    LOGGER_TRACE("finish processing trace");
    save_interval_statistics(&me.stats, args->output_path, args->cleanup);
    IntervalOlken__destroy(&me);
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
    if (args == NULL) {
        LOGGER_ERROR("bad args");
        return false;
    }

    struct EvictingMap me = {0};
    if (!EvictingMap__init(&me,
                           args->fs_shards_sampling_rate,
                           1 << 13,
                           trace->length,
                           1)) {
        LOGGER_ERROR("bad initialization");
        return false;
    }

    LOGGER_TRACE("begin processing trace with length %zu", trace->length);
    for (size_t i = 0; i < trace->length; ++i) {
        EntryType const entry = trace->trace[i].key;
        EvictingMap__access_item(&me, entry);
    }
    LOGGER_TRACE("finish processing trace");
    save_interval_statistics(&me.istats, args->emap_output_path, args->cleanup);
    EvictingMap__destroy(&me);
    return true;
}

bool
generate_fr_shards_reuse_stats(struct Trace *trace,
                               struct CommandLineArguments const *const args)
{
    LOGGER_TRACE("starting generate_fr_shards_reuse_stats(...)");
    if (trace == NULL || !implies(trace->length != 0, trace->trace != NULL)) {
        LOGGER_ERROR("invalid trace");
        return false;
    }
    if (args == NULL) {
        LOGGER_ERROR("bad args");
        return false;
    }

    struct FixedRateShards me = {0};
    // NOTE The adjustment doesn't work on the intervals because it is
    //      only performed at the end!
    if (!FixedRateShards__init(&me,
                               args->fr_shards_sampling_rate,
                               trace->length,
                               1,
                               true)) {
        LOGGER_ERROR("bad initialization");
        return false;
    }

    LOGGER_TRACE("begin processing trace with length %zu", trace->length);
    for (size_t i = 0; i < trace->length; ++i) {
        EntryType const entry = trace->trace[i].key;
        FixedRateShards__access_item(&me, entry);
    }
    LOGGER_TRACE("finish processing trace");
    save_interval_statistics(&me.istats,
                             args->fr_shards_output_path,
                             args->cleanup);
    FixedRateShards__destroy(&me);
    return true;
}

bool
generate_fs_shards_reuse_stats(struct Trace *trace,
                               struct CommandLineArguments const *const args)
{
    LOGGER_TRACE("starting generate_fs_shards_reuse_stats(...)");
    if (trace == NULL || !implies(trace->length != 0, trace->trace != NULL)) {
        LOGGER_ERROR("invalid trace");
        return false;
    }
    if (args == NULL) {
        LOGGER_ERROR("bad args");
        return false;
    }

    struct FixedSizeShards me = {0};
    // NOTE The adjustment doesn't work on the intervals because it is
    //      only performed at the end!
    if (!FixedSizeShards__init(&me,
                               args->fs_shards_sampling_rate,
                               trace->length,
                               1,
                               true)) {
        LOGGER_ERROR("bad initialization");
        return false;
    }

    LOGGER_TRACE("begin processing trace with length %zu", trace->length);
    for (size_t i = 0; i < trace->length; ++i) {
        EntryType const entry = trace->trace[i].key;
        FixedSizeShards__access_item(&me, entry);
    }
    LOGGER_TRACE("finish processing trace");
    save_interval_statistics(&me.istats,
                             args->fs_shards_output_path,
                             args->cleanup);
    FixedSizeShards__destroy(&me);
    return true;
}

static void
run_experiments(struct Trace *trace,
                struct CommandLineArguments const *const args)
{
    if (args->output_path) {
        generate_olken_reuse_stats(trace, args);
    }
    if (args->emap_output_path) {
        generate_emap_reuse_stats(trace, args);
    }
    if (args->fr_shards_output_path) {
        generate_fr_shards_reuse_stats(trace, args);
    }
    if (args->fs_shards_output_path) {
        generate_fs_shards_reuse_stats(trace, args);
    }
}

int
main(int argc, char *argv[])
{
    struct CommandLineArguments args = {0};
    if (!parse_command_line_arguments(&args, argc, argv)) {
        LOGGER_ERROR("unable to parse command line arguments");
        return EXIT_FAILURE;
    }

    if (args.input_path != NULL) {
        enum TraceFormat format = args.format;
        if (format == TRACE_FORMAT_INVALID) {
            LOGGER_ERROR("invalid trace format");
            exit(-1);
        }
        LOGGER_TRACE("beginning to read trace file '%s' with format '%s'",
                     args.input_path,
                     TRACE_FORMAT_STRINGS[args.format]);
        struct Trace trace = read_trace(args.input_path, args.format);
        run_experiments(&trace, &args);
    }
    if (args.run_uniform) {
        struct Trace trace = generate_uniform_trace(1 << 20, 1 << 20, 0);
        run_experiments(&trace, &args);
    }
    if (args.run_zipfian) {
        struct Trace trace = generate_zipfian_trace(1 << 20, 1 << 20, 0.99, 0);
        run_experiments(&trace, &args);
    }
    return 0;
}
