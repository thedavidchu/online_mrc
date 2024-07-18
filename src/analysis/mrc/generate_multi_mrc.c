/**
 *  @brief  This file provides a runner for various MRC generation algorithms.
 */
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arrays/array_size.h"
#include "evicting_map/evicting_map.h"
#include "file/file.h"
#include "glib.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "shards/fixed_rate_shards.h"
#include "shards/fixed_size_shards.h"
#include "timer/timer.h"
#include "trace/generator.h"
#include "trace/reader.h"
#include "trace/trace.h"

#include "runner_arguments.h"

struct CommandLineArguments {
    char *executable;
    gchar *input_path;
    enum TraceFormat trace_format;
    uint64_t artificial_trace_length;

    // NOTE These strings contain the following information:
    //      - Algorithm
    //      - Output MRC path
    //      - Output histogram path [optional]
    //      - Sampling rate (if applicable) [optional. Default = by algorithm]
    //      - Number of histogram bins [optional. Default = 1 << 20]
    //      - Size of histogram bins [optional. Default = 1]
    //      - Histogram overflow strategy [optional. Default = overflow]
    //      - SHARDS adjustment [optional. Default = true for Fixed-Rate SHARDS]
    gchar **run;

    // NOTE This string contains the following information:
    //      - MRC path [both input/output]
    //      - Histogram path [both input/output]
    //      - Number of histogram bins [optional. Default = arbitrarily large]
    //      - Size of histogram bins [optional. Default = 1]
    //      - Histogram overflow strategy [optional. Default = overflow]
    gchar *oracle;

    bool cleanup;
};

/// @brief  Print algorithms by name in format: "{Olken,Fixed-Rate-SHARDS,...}".
static void
print_available_algorithms(FILE *stream)
{
    fprintf(stream, "{");
    // NOTE We want to skip the "INVALID" algorithm name (i.e. 0).
    for (size_t i = 1; i < ARRAY_SIZE(algorithm_names); ++i) {
        fprintf(stream, "%s", algorithm_names[i]);
        if (i != ARRAY_SIZE(algorithm_names) - 1) {
            fprintf(stream, ",");
        }
    }
    fprintf(stream, "}");
}

static struct CommandLineArguments
parse_command_line_arguments(int argc, char *argv[])
{
    gchar *help_msg = NULL;

    // Set defaults.
    struct CommandLineArguments args = {.executable = argv[0],
                                        .input_path = NULL,
                                        .trace_format = TRACE_FORMAT_KIA,
                                        .artificial_trace_length = 1 << 20,
                                        .run = NULL,
                                        .oracle = NULL,
                                        .cleanup = FALSE};
    gchar *trace_format = NULL;

    // Command line options.
    GOptionEntry entries[] = {
        // Arguments related to the input generation
        {"input",
         'i',
         0,
         G_OPTION_ARG_FILENAME,
         &args.input_path,
         "path to the input trace",
         NULL},
        {"format",
         'f',
         0,
         G_OPTION_ARG_STRING,
         &trace_format,
         "format of the input trace. Options: {Kia,Sari}. Default: Kia.",
         NULL},
        {"length",
         'l',
         0,
         G_OPTION_ARG_INT64,
         &args.artificial_trace_length,
         "length of artificial traces. Default: 1<<20",
         NULL},
        {"run",
         'r',
         0,
         G_OPTION_ARG_STRING_ARRAY,
         &args.run,
         "arguments for various algorithm runs",
         NULL},
        {"oracle",
         'o',
         0,
         G_OPTION_ARG_STRING,
         &args.oracle,
         "arguments for the oracle",
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
    context = g_option_context_new("- generate the MRC for a trace");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        goto cleanup;
    }
    // Come on, GLib! The 'g_option_context_parse' changes the errno to
    // 2 and leaves it for me to clean up. Or maybe I'm using it wrong.
    errno = 0;

    // Check the arguments for correctness.
    if (args.input_path == NULL || (!file_exists(args.input_path) &&
                                    strcmp(args.input_path, "zipf") != 0 &&
                                    strcmp(args.input_path, "step") != 0 &&
                                    strcmp(args.input_path, "two-step") != 0)) {
        LOGGER_ERROR(
            "input trace path '%s' DNE and is not {zipf,step,two-step}",
            args.input_path == NULL ? "(null)" : args.input_path);
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
        LOGGER_TRACE("using default trace format");
    }
    if (args.run == NULL && args.oracle == NULL) {
        LOGGER_ERROR("expected at least some work!");
        goto cleanup;
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

/// @note   TIL GLib's strings from the command line parsing must be
///         manually freed. Of course it makes sense, but I am just a
///         silly-willy.
void
free_command_line_arguments(struct CommandLineArguments *const args)
{
    g_free(args->input_path);
    g_free(args->oracle);
    if (args->run) {
        for (size_t i = 0; args->run[i] != NULL; ++i) {
            g_free(args->run[i]);
        }
        g_free(args->run);
    }
    *args = (struct CommandLineArguments){0};
}

static void
print_command_line_arguments(struct CommandLineArguments const *args)
{
    LOGGER_INFO("CommandLineArguments(executable='%s', ...)", args->executable);
    // fprintf(stderr,
    //         "CommandLineArguments(executable='%s', input_path='%s', "
    //         "algorithm='%s', output_path='%s', shards_ratio='%g', "
    //         "artifical_trace_length='%zu', oracle_path='%s', "
    //         "hist_num_bins=%zu, hist_bin_size=%zu, cleanup=%s)\n",
    //         args->executable,
    //         args->input_path,
    //         algorithm_names[args->algorithm],
    //         args->output_path,
    //         args->shards_sampling_ratio,
    //         args->artificial_trace_length,
    //         args->oracle_path ? args->oracle_path : "(null)",
    //         args->hist_num_bins,
    //         args->hist_bin_size,
    //         bool_to_string(args->cleanup));
}

static void
print_trace_summary(struct CommandLineArguments const *args,
                    struct Trace const *const trace)
{
    fprintf(LOGGER_STREAM,
            "Trace(source='%s', format='%s', length=%zu)\n",
            args->input_path,
            TRACE_FORMAT_STRINGS[args->trace_format],
            trace->length);
}

static inline bool
trace_runner(void *const runner_data,
             struct RunnerArguments const *const args,
             struct Trace const *const trace,
             bool (*access_func)(void *const, uint64_t const),
             bool (*postprocess_func)(void *const),
             bool (*hist_func)(void *const, struct Histogram const **const),
             void (*destroy_func)(void *const))
{
    struct MissRateCurve mrc = {0};
    struct Histogram const *hist = NULL;

    double t0 = get_wall_time_sec();
    for (size_t i = 0; i < trace->length; ++i) {
        // NOTE I really, really, really hope that the compiler is smart
        //      enough to inline this function!!!
        access_func(runner_data, trace->trace[i].key);
        if (i % 1000000 == 0) {
            LOGGER_TRACE("Finished %zu / %zu", i, trace->length);
        }
    }
    double t1 = get_wall_time_sec();
    postprocess_func(runner_data);
    double t2 = get_wall_time_sec();
    // NOTE We do NOT own the histogram data through the 'hist' object.
    //      The 'runner_data' object maintains ownership of the data.
    if (!hist_func(runner_data, &hist)) {
        LOGGER_ERROR("histogram getter failed");
        goto cleanup;
    }
    if (!MissRateCurve__init_from_histogram(&mrc, hist)) {
        LOGGER_ERROR("MRC initialization failed");
        goto cleanup;
    }
    double t3 = get_wall_time_sec();
    LOGGER_INFO("%s -- Histogram Time: %f | Post-Process Time: %f | MRC Time: "
                "%f | Total Time: %f",
                algorithm_names[args->algorithm],
                (double)(t1 - t0),
                (double)(t2 - t1),
                (double)(t3 - t2),
                (double)(t3 - t0));
    if (args->hist_path != NULL) {
        if (!Histogram__save(hist, args->hist_path)) {
            LOGGER_WARN("failed to save histogram in '%s'", args->hist_path);
        }
    }
    if (args->mrc_path != NULL) {
        if (!MissRateCurve__save(&mrc, args->mrc_path)) {
            LOGGER_WARN("failed to save MRC in '%s'", args->mrc_path);
        }
    }

    destroy_func(runner_data);
    MissRateCurve__destroy(&mrc);
    return true;
cleanup:
    destroy_func(runner_data);
    MissRateCurve__destroy(&mrc);
    return false;
}

static bool
run_olken(struct RunnerArguments const *const args,
          struct Trace const *const trace)
{
    struct Olken me = {0};
    if (!Olken__init_full(&me,
                          args->num_bins,
                          args->bin_size,
                          args->out_of_bounds_mode)) {
        LOGGER_ERROR("initialization failed!");
        return false;
    }

    return trace_runner(
        &me,
        args,
        trace,
        (bool (*)(void *const, uint64_t const))Olken__access_item,
        (bool (*)(void *const))Olken__post_process,
        (bool (*)(void *const,
                  struct Histogram const **const))Olken__get_histogram,
        (void (*)(void *const))Olken__destroy);
}

static bool
run_fixed_rate_shards(struct RunnerArguments const *const args,
                      struct Trace const *const trace)
{
    struct FixedRateShards me = {0};
    if (!FixedRateShards__init_full(&me,
                                    args->sampling_rate,
                                    args->num_bins,
                                    args->bin_size,
                                    args->out_of_bounds_mode,
                                    args->shards_adj)) {
        LOGGER_ERROR("initialization failed!");
        return false;
    }

    return trace_runner(
        &me,
        args,
        trace,
        (bool (*)(void *const, uint64_t const))FixedRateShards__access_item,
        (bool (*)(void *const))FixedRateShards__post_process,
        (bool (*)(void *const, struct Histogram const **const))
            FixedRateShards__get_histogram,
        (void (*)(void *const))FixedRateShards__destroy);
}

static bool
run_fixed_size_shards(struct RunnerArguments const *const args,
                      struct Trace const *const trace)
{
    struct FixedSizeShards me = {0};
    if (!FixedSizeShards__init_full(&me,
                                    args->sampling_rate,
                                    args->max_size,
                                    args->num_bins,
                                    args->bin_size,
                                    args->out_of_bounds_mode)) {
        LOGGER_ERROR("initialization failed!");
        return false;
    }

    return trace_runner(
        &me,
        args,
        trace,
        (bool (*)(void *const, uint64_t const))FixedSizeShards__access_item,
        (bool (*)(void *const))FixedSizeShards__post_process,
        (bool (*)(void *const, struct Histogram const **const))
            FixedSizeShards__get_histogram,
        (void (*)(void *const))FixedSizeShards__destroy);
}

static bool
run_evicting_map(struct RunnerArguments const *const args,
                 struct Trace const *const trace)
{
    struct EvictingMap me = {0};
    if (!EvictingMap__init_full(&me,
                                args->sampling_rate,
                                args->max_size,
                                args->num_bins,
                                args->bin_size,
                                args->out_of_bounds_mode)) {
        LOGGER_ERROR("initialization failed!");
        return false;
    }

    return trace_runner(
        &me,
        args,
        trace,
        (bool (*)(void *const, uint64_t const))EvictingMap__access_item,
        (bool (*)(void *const))EvictingMap__post_process,
        (bool (*)(void *const,
                  struct Histogram const **const))EvictingMap__get_histogram,
        (void (*)(void *const))EvictingMap__destroy);
}

/// @note   I introduce this function so that I can do perform some logic but
///         also maintain the constant-qualification of the members of struct
///         Trace.
static struct Trace
get_trace(struct CommandLineArguments args)
{
    if (strcmp(args.input_path, "zipf") == 0) {
        LOGGER_TRACE("Generating artificial Zipfian trace");
        return generate_zipfian_trace(args.artificial_trace_length,
                                      args.artificial_trace_length,
                                      0.99,
                                      0);
    } else if (strcmp(args.input_path, "step") == 0) {
        LOGGER_TRACE("Generating artificial step function trace");
        return generate_step_trace(args.artificial_trace_length,
                                   args.artificial_trace_length / 10);
    } else if (strcmp(args.input_path, "two-step") == 0) {
        LOGGER_TRACE("Generating artificial two-step function trace");
        return generate_two_step_trace(args.artificial_trace_length,
                                       args.artificial_trace_length / 10);
    } else if (strcmp(args.input_path, "two-distr") == 0) {
        LOGGER_TRACE("Generating artificial two-distribution function trace");
        return generate_two_distribution_trace(args.artificial_trace_length,
                                               args.artificial_trace_length /
                                                   10);
    } else {
        LOGGER_TRACE("Reading trace from '%s'", args.input_path);
        return read_trace(args.input_path, args.trace_format);
    }
}

struct RunnerArgumentsArray {
    struct RunnerArguments *data;
    size_t length;
};

static struct RunnerArgumentsArray
create_work_array(struct CommandLineArguments const *const args)
{
    // Get length of allocation required
    size_t length = 0;
    if (args->oracle != NULL) {
        ++length;
    }
    if (args->run != NULL) {
        for (size_t i = 0; args->run[i] != NULL; ++i) {
            ++length;
        }
    }
    struct RunnerArguments *array = calloc(length, sizeof(*array));
    assert(array != NULL);

    // Initialize work data
    if (args->oracle != NULL) {
        if (!RunnerArguments__init(&array[0], args->oracle)) {
            LOGGER_FATAL("failed to initialize runner arguments '%s'",
                         args->oracle);
            exit(-1);
        }
    }
    if (args->run != NULL) {
        for (size_t i = 0; args->run[i] != NULL; ++i) {
            // NOTE I have to check whether the oracle was provided so
            //      I know if I take the first bucket or the second.
            //      Maybe in the future, I should just _always_ reserve
            //      the first bucket for the oracle regardless of
            //      whether it actually uses it.
            if (!RunnerArguments__init(
                    &array[(args->oracle != NULL ? 1 : 0) + i],
                    args->run[i])) {
                LOGGER_FATAL("failed to initialize runner arguments '%s'",
                             args->run[i]);
                exit(-1);
            }
        }
    }

    return (struct RunnerArgumentsArray){.data = array, .length = length};
}

static void
free_work_array(struct RunnerArgumentsArray *me)
{
    if (me == NULL) {
        return;
    }
    for (size_t i = 0; i < me->length; ++i) {
        RunnerArguments__destroy(&me->data[i]);
    }
    free(me->data);
    *me = (struct RunnerArgumentsArray){0};
}

static bool
run_runner(struct RunnerArguments const *const args,
           struct Trace const *const trace)
{
    if (!args->ok) {
        // NOTE I have a bunch of checks in place so this shouldn't
        //      ever trigger unless someone calls this function from
        //      another way.
        LOGGER_WARN("skipping because it's not ok");
        return false;
    }
    RunnerArguments__println(args, LOGGER_STREAM);
    switch (args->algorithm) {
    case MRC_ALGORITHM_OLKEN:
        if (!run_olken(args, trace)) {
            LOGGER_WARN("Olken failed");
        }
        return true;
    case MRC_ALGORITHM_FIXED_RATE_SHARDS:
        if (!run_fixed_rate_shards(args, trace)) {
            LOGGER_WARN("Fixed-Rate SHARDS failed");
        }
        return true;
    case MRC_ALGORITHM_FIXED_SIZE_SHARDS:
        if (!run_fixed_size_shards(args, trace)) {
            LOGGER_WARN("Fixed-Size SHARDS failed");
        }
        return true;
    case MRC_ALGORITHM_EVICTING_MAP:
        if (!run_evicting_map(args, trace)) {
            LOGGER_WARN("Evicting Map failed");
        }
        return true;
    case MRC_ALGORITHM_QUICKMRC:
    case MRC_ALGORITHM_GOEL_QUICKMRC:
    case MRC_ALGORITHM_AVERAGE_EVICTION_TIME:
    case MRC_ALGORITHM_THEIR_AVERAGE_EVICTION_TIME:
        LOGGER_WARN("not implemented algorithm %s",
                    algorithm_names[args->algorithm]);
        return false;
    default:
        LOGGER_WARN("invalid algorithm %s", algorithm_names[args->algorithm]);
        fprintf(LOGGER_STREAM, "algorithms include: ");
        print_available_algorithms(LOGGER_STREAM);
        fprintf(LOGGER_STREAM, "\n");
        return false;
    }
}

static bool
run_cleanup(struct RunnerArguments const *const args)
{
    LOGGER_TRACE("cleaning up '%s' and '%s'",
                 maybe_string(args->hist_path),
                 maybe_string(args->mrc_path));
    // NOTE This takes advantage of C's short-circuiting booleans.
    //      I find this idiom cleaner than nested if-statements.
    if (args->hist_path != NULL && remove(args->hist_path) != 0) {
        LOGGER_WARN("failed to remove '%s'", args->hist_path);
    }
    if (args->mrc_path != NULL && remove(args->mrc_path) != 0) {
        LOGGER_WARN("failed to remove '%s'", args->mrc_path);
    }
    return true;
}

int
main(int argc, char **argv)
{
    // This variable is for things that are not critical failures but
    // indicate we didn't succeed.
    int status = EXIT_SUCCESS;
    struct CommandLineArguments args = {0};
    args = parse_command_line_arguments(argc, argv);
    print_command_line_arguments(&args);

    // Read in trace
    double t0 = get_wall_time_sec();
    struct Trace trace = get_trace(args);
    double t1 = get_wall_time_sec();
    LOGGER_INFO("Trace Read Time: %f sec", t1 - t0);
    if (trace.trace == NULL || trace.length == 0) {
        // I cast to (void *) so that it doesn't complain about printing it.
        LOGGER_ERROR("invalid trace {.trace = %p, .length = %zu}",
                     (void *)trace.trace,
                     trace.length);
        return EXIT_FAILURE;
    }
    print_trace_summary(&args, &trace);

    struct RunnerArgumentsArray work = create_work_array(&args);
    for (size_t i = 0; i < work.length; ++i) {
        // TODO(dchu)   We want to avoid rerunning the (expensive)
        //              oracle if the files already exist.
        if (!run_runner(&work.data[i], &trace)) {
            LOGGER_ERROR("oracle runner failed");
            status = EXIT_FAILURE;
        }
    }

    // Optionally check MAE and MSE
    if (args.oracle != NULL) {
        LOGGER_TRACE("Comparing against oracle");
        struct MissRateCurve oracle_mrc = {0};
        if (!MissRateCurve__load(&oracle_mrc, work.data[0].mrc_path)) {
            LOGGER_ERROR("failed to load oracle MRC");
            exit(EXIT_FAILURE);
        }

        // NOTE We start from 1 because the first spot is occupied by
        //      the oracle. We know this for sure because the oracle is
        //      not NULL. However, I admit that this is confusing and
        //      therefore sketchy.
        for (size_t i = 1; i < work.length; ++i) {
            struct MissRateCurve mrc = {0};
            if (!MissRateCurve__load(&mrc, work.data[i].mrc_path)) {
                LOGGER_ERROR("failed to load MRC from '%s'",
                             work.data[i].mrc_path);
                status = EXIT_FAILURE;
                continue;
            }
            double mse = MissRateCurve__mean_squared_error(&oracle_mrc, &mrc);
            double mae = MissRateCurve__mean_absolute_error(&oracle_mrc, &mrc);
            LOGGER_INFO("%s -- Mean Absolute Error (MAE): %f | Mean Squared "
                        "Error (MSE): %f",
                        work.data[i].mrc_path,
                        mae,
                        mse);
            MissRateCurve__destroy(&mrc);
        }
        MissRateCurve__destroy(&oracle_mrc);
    }

    if (args.cleanup) {
        for (size_t i = 0; i < work.length; ++i) {
            if (!run_cleanup(&work.data[i])) {
                LOGGER_ERROR("oracle runner failed");
                status = EXIT_FAILURE;
            }
        }
    }

    free_work_array(&work);
    free_command_line_arguments(&args);
    Trace__destroy(&trace);

    return status;
}
