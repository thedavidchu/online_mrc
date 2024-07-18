/**
 *  @brief  This file generates the oracle for an MRC trace.
 *
 *  @note   Use the 'run_mrc_generators.py' script as a convenient wrapper!
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <glib.h>

#include "file/file.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "timer/timer.h"
#include "trace/generator.h"
#include "trace/reader.h"
#include "trace/trace.h"

struct CommandLineArguments {
    char *executable;

    // Inputs
    char *trace_path;
    enum TraceFormat trace_format;

    // Outputs
    char *histogram_path;
    char *mrc_path;

    uint64_t num_bins;
    uint64_t bin_size;

    gboolean cleanup;
};

static void
print_trace_summary(struct CommandLineArguments const *args,
                    struct Trace const *const trace)
{
    fprintf(stderr,
            "Trace(source='%s', format='%s', length=%zu)\n",
            args->trace_path,
            TRACE_FORMAT_STRINGS[args->trace_format],
            trace->length);
}

static struct CommandLineArguments
parse_command_line_arguments(int argc, char **argv)
{
    gchar *help_msg = NULL;

    // Set defaults.
    struct CommandLineArguments args = {.executable = argv[0],
                                        .trace_path = NULL,
                                        .trace_format = TRACE_FORMAT_KIA,
                                        .num_bins = 1 << 20,
                                        .bin_size = 1,
                                        .histogram_path = "histogram.bin",
                                        .mrc_path = "mrc.bin",
                                        .cleanup = FALSE};
    gchar *trace_format = NULL;

    // Command line options.
    GOptionEntry entries[] = {
        {"input",
         'i',
         0,
         G_OPTION_ARG_FILENAME,
         &args.trace_path,
         "path to the input trace",
         NULL},
        {"format",
         'f',
         0,
         G_OPTION_ARG_STRING,
         &trace_format,
         "format of the input trace. Options: {Kia,Sari}. Default: Kia.",
         NULL},
        {"histogram",
         // NOTE I chose 'g' because the second syllable of 'histogram'
         //      begins with a 'g'. Cryptic, I know... Needless to say,
         //     '-h' was taken by the help option.
         'g',
         0,
         G_OPTION_ARG_FILENAME,
         &args.histogram_path,
         "path to the output histogram. Default: 'histogram.bin'.",
         NULL},
        {"mrc",
         'm',
         0,
         G_OPTION_ARG_FILENAME,
         &args.mrc_path,
         "path to the output MRC. Default: 'mrc.bin'.",
         NULL},
        {"num-bins",
         'n',
         0,
         G_OPTION_ARG_INT64,
         &args.num_bins,
         "initial number of bins to use. Note: this affects performance but "
         "not accuracy. Default: 1 << 20.",
         NULL},
        {"bin-size",
         'b',
         0,
         G_OPTION_ARG_INT64,
         &args.bin_size,
         "size of the histogram and MRC bins. Note: this affects performance "
         "and precision. Default: 1.",
         NULL},
        {"cleanup",
         0,
         0,
         G_OPTION_ARG_NONE,
         &args.cleanup,
         "cleanup generated files afterward",
         NULL},
        G_OPTION_ENTRY_NULL,
    };

    GError *error = NULL;
    GOptionContext *context;
    context = g_option_context_new("- analyze MRC in intervals");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        goto cleanup;
    }
    // Come on, GLib! The 'g_option_context_parse' changes the errno to
    // 2 and leaves it for me to clean up. Or maybe I'm using it wrong.
    errno = 0;

    // Check the arguments for correctness.
    if (args.trace_path == NULL) {
        LOGGER_ERROR("input trace path '%s' DNE", args.trace_path);
        goto cleanup;
    }
    if (!file_exists(args.trace_path)) {
        LOGGER_ERROR("input trace path '%s' DNE", args.trace_path);
        goto cleanup;
    }
    if (trace_format != NULL) {
        args.trace_format = parse_trace_format_string(trace_format);
        if (args.trace_format == TRACE_FORMAT_INVALID) {
            LOGGER_ERROR("invalid trace format '%s'", trace_format);
            goto cleanup;
        }
    } else {
        // NOTE If 'trace_format' is NULL, the we remain with the default.
        LOGGER_INFO("using default trace format");
    }
    if (args.num_bins <= 0) {
        LOGGER_ERROR("number of bins %" PRIu64 " must be positive");
        goto cleanup;
    }
    if (args.num_bins <= 0) {
        LOGGER_ERROR("bin size %" PRIu64 " must be positive");
        goto cleanup;
    }

    // Emit warnings.
    if (file_exists(args.histogram_path)) {
        LOGGER_WARN("histogram file '%s' already exists", args.histogram_path);
    }
    if (file_exists(args.mrc_path)) {
        LOGGER_WARN("MRC file '%s' already exists", args.mrc_path);
    }

    g_option_context_free(context);
    return args;
cleanup:
    help_msg = g_option_context_get_help(context, FALSE, NULL);
    g_print("%s", help_msg);
    free(help_msg);
    g_option_context_free(context);
    exit(-1);
}

static bool
get_trace(struct Trace *const trace, struct CommandLineArguments const args)
{
    LOGGER_TRACE("Reading trace from '%s'", args.trace_path);
    double t0 = get_wall_time_sec();
    *trace = read_trace(args.trace_path, args.trace_format);
    double t1 = get_wall_time_sec();
    LOGGER_INFO("Trace Read Time: %f sec", t1 - t0);
    if (trace->trace == NULL || trace->length == 0) {
        // I cast to (void *) so that it doesn't complain about printing it.
        LOGGER_ERROR("invalid trace {.trace = %p, .length = %zu}",
                     (void *)trace->trace,
                     trace->length);
        return false;
    }
    return true;
}

static bool
run_olken(struct CommandLineArguments const *args,
          struct Trace const *const trace)
{
    struct Olken me = {0};
    struct MissRateCurve mrc = {0};

    if (!Olken__init_full(&me,
                          args->num_bins,
                          args->bin_size,
                          HistogramOutOfBoundsMode__realloc)) {
        LOGGER_ERROR("failed to initialize");
        goto cleanup;
    }
    LOGGER_TRACE("Begin running trace");
    double t0 = get_wall_time_sec();
    for (size_t i = 0; i < trace->length; ++i) {
        Olken__access_item(&me, trace->trace[i].key);
        if (i % 1000000 == 0) {
            LOGGER_TRACE("Finished %zu / %zu", i, trace->length);
        }
    }
    double t1 = get_wall_time_sec();
    LOGGER_TRACE("Begin post process");
    if (!Olken__post_process(&me)) {
        LOGGER_ERROR("failed to post-process");
        goto cleanup;
    }
    double t2 = get_wall_time_sec();
    if (!Olken__to_mrc(&me, &mrc)) {
        LOGGER_ERROR("failed to create MRC");
        goto cleanup;
    }
    double t3 = get_wall_time_sec();
    LOGGER_INFO("Histogram Time: %f | Post-Process Time: %f | MRC Time: "
                "%f | Total Time: %f",
                (double)(t1 - t0),
                (double)(t2 - t1),
                (double)(t3 - t2),
                (double)(t3 - t0));
    LOGGER_TRACE("Saving histogram to '%s'", args->histogram_path);
    Histogram__save(&me.histogram, args->histogram_path);
    LOGGER_TRACE("Done saving histogram to '%s'", args->histogram_path);
    LOGGER_TRACE("Saving MRC to '%s'", args->mrc_path);
    MissRateCurve__save(&mrc, args->mrc_path);
    LOGGER_TRACE("Done saving MRC to '%s'", args->mrc_path);
    Olken__destroy(&me);
    return true;
cleanup:
    Olken__destroy(&me);
    MissRateCurve__destroy(&mrc);
    return false;
}

int
main(int argc, char **argv)
{
    struct CommandLineArguments args = {0};
    args = parse_command_line_arguments(argc, argv);

    // Read in trace
    struct Trace trace = {0};
    if (!get_trace(&trace, args)) {
        LOGGER_ERROR("trace reader failed");
        return EXIT_FAILURE;
    }
    print_trace_summary(&args, &trace);

    // Run Olken
    if (!run_olken(&args, &trace)) {
        LOGGER_ERROR("Olken failed");
        return EXIT_FAILURE;
    }

    // Cleanup
    Trace__destroy(&trace);
    if (args.cleanup) {
        remove(args.histogram_path);
        remove(args.mrc_path);
    }
    return 0;
}
