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

#include "file/file.h"
#include "glib.h"
#include "logger/logger.h"
#include "lookup/dictionary.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "timer/timer.h"
#include "trace/generator.h"
#include "trace/reader.h"
#include "trace/trace.h"

#include "run/helper.h"
#include "run/run_oracle.h"
#include "run/runner_arguments.h"
#include "run/trace_runner.h"

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
    fprintf(LOGGER_STREAM,
            "CommandLineArguments(executable='%s', input='%s', format='%s', "
            "length=%zu, oracle='%s', run=",
            args->executable,
            args->input_path,
            TRACE_FORMAT_STRINGS[args->trace_format],
            args->artificial_trace_length,
            maybe_string(args->oracle));
    if (args->run != NULL) {
        fprintf(LOGGER_STREAM, "[");
        for (size_t i = 0; args->run[i] != NULL; ++i) {
            fprintf(LOGGER_STREAM, "'%s'", args->run[i]);
            if (args->run[i + 1] != NULL) {
                fprintf(LOGGER_STREAM, ",");
            }
        }
        fprintf(LOGGER_STREAM, "]");
    } else {
        fprintf(LOGGER_STREAM, "null");
    }
    fprintf(LOGGER_STREAM, ")\n");
}

static void
print_trace_summary(struct CommandLineArguments const *args,
                    struct Trace const *const trace)
{
    fprintf(LOGGER_STREAM,
            "Trace(source='%s', format='%s', trace=%p, length=%zu)\n",
            args->input_path,
            TRACE_FORMAT_STRINGS[args->trace_format],
            (void *)trace->trace,
            trace->length);
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
    // This is a single argument pointer.
    struct RunnerArguments *oracle_arg;
    // This is an array of arguments.
    struct RunnerArguments *data;
    size_t length;
};

/// @brief  Return true iff both paths are non-NULL and match.
static bool
string_is_unique(struct Dictionary *const dict, char const *const output_path)
{
    if (output_path == NULL) {
        return true;
    } else if (Dictionary__contains(dict, output_path)) {
        LOGGER_ERROR("output path already used: '%s'", output_path);
        return false;
    } else {
        Dictionary__put(dict, output_path, "");
        return true;
    }
}

/// @note   This does not do anti-aliasing of paths, so you still may
///         end up overwriting your paths!
///         Example: '././blah' and './blah' refer to the same file.
static bool
check_no_paths_match(struct RunnerArguments const *const oracle_arg,
                     struct RunnerArguments const *const array,
                     size_t const length)
{
    bool ok = true;
    struct Dictionary dict = {0};
    if (!Dictionary__init(&dict)) {
        LOGGER_ERROR("failed to init dictionary");
        return false;
    }
    if (oracle_arg != NULL) {
        if (!string_is_unique(&dict, oracle_arg->mrc_path) ||
            !string_is_unique(&dict, oracle_arg->hist_path)) {
            LOGGER_ERROR("oracle has matching output path");
            ok = false;
        }
    }
    for (size_t i = 0; i < length; ++i) {
        if (!string_is_unique(&dict, array[i].mrc_path) ||
            !string_is_unique(&dict, array[i].hist_path)) {
            LOGGER_ERROR("something has matching output path");
            ok = false;
        }
    }
    Dictionary__destroy(&dict);
    return ok;
}

/// @note   This isn't actually that bad except for the fact that the
///         logging isn't specific enough. I do this simply for the
///         convenience of not having to think too hard about how to
///         resolve this.
/// @todo   I should check that the oracle's algorithm is no used either
static bool
check_no_algorithms_match(struct RunnerArguments const *const oracle_arg,
                          struct RunnerArguments const *const array,
                          size_t const length)
{
    bool ok = true;
    struct Dictionary dict = {0};
    if (!Dictionary__init(&dict)) {
        LOGGER_ERROR("failed to init dictionary");
        return false;
    }
    if (oracle_arg != NULL) {
        char const *const algo = algorithm_names[oracle_arg->algorithm];
        if (!string_is_unique(&dict, algo)) {
            LOGGER_ERROR("algorithm '%s' is run twice", algo);
            ok = false;
        }
    }
    for (size_t i = 0; i < length; ++i) {
        char const *const algo = algorithm_names[array[i].algorithm];
        if (!string_is_unique(&dict, algo)) {
            LOGGER_ERROR("algorithm '%s' is run twice", algo);
            ok = false;
        }
    }
    Dictionary__destroy(&dict);
    return ok;
}

static void
free_work_array(struct RunnerArgumentsArray *me)
{
    if (me == NULL) {
        return;
    }
    RunnerArguments__destroy(me->oracle_arg);
    for (size_t i = 0; i < me->length; ++i) {
        RunnerArguments__destroy(&me->data[i]);
    }
    free(me->oracle_arg);
    free(me->data);
    *me = (struct RunnerArgumentsArray){0};
}

static struct RunnerArgumentsArray
create_work_array(struct CommandLineArguments const *const args)
{
    struct RunnerArgumentsArray r = {0};
    if (args->oracle != NULL) {
        r.oracle_arg = calloc(1, sizeof(*r.oracle_arg));
        if (r.oracle_arg == NULL) {
            LOGGER_ERROR("bad calloc(%zu, %zu)", 1, sizeof(*r.data));
            goto cleanup;
        }
        if (!RunnerArguments__init(r.oracle_arg, args->oracle)) {
            LOGGER_FATAL("failed to initialize runner arguments '%s'",
                         args->oracle);
            goto cleanup;
        }
        if (r.oracle_arg->algorithm != MRC_ALGORITHM_OLKEN &&
            r.oracle_arg->algorithm != MRC_ALGORITHM_ORACLE) {
            LOGGER_ERROR(
                "Oracle algorithm must be 'Oracle' or 'Olken', not '%s'",
                algorithm_names[r.oracle_arg->algorithm]);
            goto cleanup;
        }
    }
    if (args->run != NULL) {
        // Get length of allocation required
        size_t length = 0;
        for (size_t i = 0; args->run[i] != NULL; ++i) {
            ++length;
        }
        r.data = calloc(length, sizeof(*r.data));
        if (r.data == NULL) {
            LOGGER_ERROR("bad calloc(%zu, %zu)", length, sizeof(*r.data));
            goto cleanup;
        }
        r.length = length;
        for (size_t i = 0; args->run[i] != NULL; ++i) {
            if (!RunnerArguments__init(&r.data[i], args->run[i])) {
                LOGGER_FATAL("failed to initialize runner arguments '%s'",
                             args->run[i]);
                exit(-1);
            }
            if (r.data[i].algorithm == MRC_ALGORITHM_ORACLE) {
                LOGGER_ERROR("regular algorithm cannot be 'Oracle'");
                goto cleanup;
            }
        }
    }

    // Verify the work array is valid and makes sense
    if (!check_no_paths_match(r.oracle_arg, r.data, r.length)) {
        LOGGER_ERROR("matching paths means we'd overwrite some values!");
        goto cleanup;
    }
    if (!check_no_algorithms_match(r.oracle_arg, r.data, r.length)) {
        LOGGER_ERROR(
            "matching algorithms means our logging won't be specific enough!");
        goto cleanup;
    }
    // TODO(dchu)   Add a warning when parameters are not used. For example,
    //              'Olken' does not use 'sampling' or 'max_size'.
    return r;
cleanup:
    // NOTE It is OK to cleanup the array because calloc set all the
    //      pointers to NULL so we are not doing anything bad.
    free_work_array(&r);
    // NOTE The 'free_work_array()' function should set 'r' to '{0}',
    //      but I want to be very explicit that it will be '{0}'.
    return (struct RunnerArgumentsArray){0};
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
    bool ok = true;
    struct CommandLineArguments args = {0};
    args = parse_command_line_arguments(argc, argv);
    print_command_line_arguments(&args);

    // Parse work. This is above the trace reader because it should be
    // faster and thus a failure will fail faster.
    struct RunnerArgumentsArray work = create_work_array(&args);
    if (work.oracle_arg == NULL && work.data == NULL && work.length == 0) {
        LOGGER_INFO("error in creating work array");
        goto cleanup_cmdln;
    }

    if (work.oracle_arg != NULL &&
        work.oracle_arg->algorithm == MRC_ALGORITHM_ORACLE) {
        run_oracle(args.input_path, args.trace_format, work.oracle_arg);
    }

    // Read in trace. This can be a very slow process.
    double const t0 = get_wall_time_sec();
    struct Trace trace = get_trace(args);
    double const t1 = get_wall_time_sec();
    LOGGER_INFO("Trace Read Time: %f sec", t1 - t0);
    if (trace.trace == NULL || trace.length == 0) {
        // I cast to (void *) so that it doesn't complain about printing it.
        LOGGER_ERROR("invalid trace {.trace = %p, .length = %zu}",
                     (void *)trace.trace,
                     trace.length);
        goto cleanup_work;
    }
    print_trace_summary(&args, &trace);

    if (work.oracle_arg != NULL &&
        work.oracle_arg->algorithm == MRC_ALGORITHM_OLKEN) {
        if (!run_runner(work.oracle_arg, &trace)) {
            LOGGER_ERROR("trace runner failed");
            ok = false;
        }
    }
    for (size_t i = 0; i < work.length; ++i) {
        if (!run_runner(&work.data[i], &trace)) {
            LOGGER_ERROR("trace runner failed");
            ok = false;
        }
    }

    // Optionally check MAE and MSE -- but only if '--oracle' was specified!
    if (args.oracle != NULL) {
        LOGGER_TRACE("Comparing against oracle");
        struct MissRateCurve oracle_mrc = {0};
        char const *const oracle_mrc_path = work.oracle_arg->mrc_path;
        if (!MissRateCurve__load(&oracle_mrc, oracle_mrc_path)) {
            LOGGER_ERROR("failed to load oracle MRC at '%s'",
                         oracle_mrc_path ? oracle_mrc_path : "(null)");
            goto cleanup_trace;
        }

        for (size_t i = 0; i < work.length; ++i) {
            char const *const mrc_path = work.data[i].mrc_path;
            struct MissRateCurve mrc = {0};
            if (!MissRateCurve__load(&mrc, mrc_path)) {
                LOGGER_ERROR("failed to load MRC from '%s'",
                             mrc_path ? mrc_path : "(null)");
                ok = false;
                continue;
            }
            double mse = MissRateCurve__mean_squared_error(&oracle_mrc, &mrc);
            double mae = MissRateCurve__mean_absolute_error(&oracle_mrc, &mrc);
            LOGGER_INFO("%s -- Mean Absolute Error (MAE): %f | Mean Squared "
                        "Error (MSE): %f",
                        algorithm_names[work.data[i].algorithm],
                        mae,
                        mse);
            MissRateCurve__destroy(&mrc);
        }
        MissRateCurve__destroy(&oracle_mrc);
    }

    // NOTE We clean up the MRC and histogram files when we test because
    //      we don't like to pollute our file system every time we run
    //      our tests!
    if (args.cleanup) {
        for (size_t i = 0; i < work.length; ++i) {
            if (!run_cleanup(&work.data[i])) {
                LOGGER_ERROR("cleanup failed");
                ok = false;
            }
        }
    }

    free_work_array(&work);
    free_command_line_arguments(&args);
    Trace__destroy(&trace);

    if (!ok) {
        goto cleanup;
    }
    LOGGER_INFO("=== SUCCESS ===");
    return EXIT_SUCCESS;
cleanup_trace:
    Trace__destroy(&trace);
cleanup_work:
    free_work_array(&work);
cleanup_cmdln:
    free_command_line_arguments(&args);
cleanup:
    // NOTE I don't cleanup the memory because I count on the OS doing
    //      it and because some of the goto statements jump over
    //      variable declarations, which would cause errors.
    LOGGER_ERROR("=== FAILURE ===");
    return EXIT_FAILURE;
}
