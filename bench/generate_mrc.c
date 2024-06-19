/**
 *  @brief  This file provides a runner for various MRC generation algorithms.
 */
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arrays/array_size.h"
#include "average_eviction_time/average_eviction_time.h"
#include "evicting_map/evicting_map.h"
#include "goel_quickmrc/goel_quickmrc.h"
#include "histogram/histogram.h"
#include "io/io.h"
#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "quickmrc/quickmrc.h"
#include "shards/fixed_rate_shards.h"
#include "shards/fixed_size_shards.h"
#include "timer/timer.h"
#include "trace/generator.h"
#include "trace/reader.h"
#include "trace/trace.h"
#include "unused/mark_unused.h"

static const size_t DEFAULT_ARTIFICIAL_TRACE_LENGTH = 1 << 20;
static const double DEFAULT_SHARDS_SAMPLING_RATIO = 1e-3;
static char *DEFAULT_ORACLE_PATH = NULL;
static const size_t DEFAULT_HIST_BIN_SIZE = 1;
static char const *DEFAULT_HISTOGRAM_PATH = NULL;

static char const *const BOOLEAN_STRINGS[2] = {"false", "true"};

enum MRCAlgorithm {
    MRC_ALGORITHM_INVALID,
    MRC_ALGORITHM_OLKEN,
    MRC_ALGORITHM_FIXED_RATE_SHARDS,
    MRC_ALGORITHM_FIXED_RATE_SHARDS_ADJ,
    MRC_ALGORITHM_FIXED_SIZE_SHARDS,
    MRC_ALGORITHM_QUICKMRC,
    MRC_ALGORITHM_GOEL_QUICKMRC,
    MRC_ALGORITHM_EVICTING_MAP,
    MRC_ALGORITHM_AVERAGE_EVICTION_TIME,
    MRC_ALGORITHM_THEIR_AVERAGE_EVICTION_TIME,
};

// NOTE This corresponds to the same order as MRCAlgorithm so that we can
//      simply use the enumeration to print the correct string!
static char *algorithm_names[] = {
    "INVALID",
    "Olken",
    "Fixed-Rate-SHARDS",
    "Fixed-Rate-SHARDS-Adj",
    "Fixed-Size-SHARDS",
    "QuickMRC",
    "Goel-QuickMRC",
    "Evicting-Map",
    "Average-Eviction-Time",
    "Their-Average-Eviction-Time",
};

struct CommandLineArguments {
    char *executable;
    enum MRCAlgorithm algorithm;
    char *input_path;
    enum TraceFormat trace_format;
    char *output_path;

    double shards_sampling_ratio;
    size_t artificial_trace_length;

    char *oracle_path;

    size_t hist_bin_size;
    char *hist_output_path;
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

static inline char const *
bool_to_string(bool x)
{
    return x ? BOOLEAN_STRINGS[1] : BOOLEAN_STRINGS[0];
}

static void
print_help(FILE *stream, struct CommandLineArguments const *args)
{
    fprintf(stream,
            "Usage:\n"
            "    %s \\\n"
            "        --input|-i <input-path> --algorithm|-a <algorithm> "
            "--output|-o <output-path> \\\n"
            "        [--sampling-ratio|-s <ratio>] \\\n"
            "        [--number-entries|-n <trace-length>] \\\n"
            "        [--oracle <oracle-path>] \\\n"
            "        [--hist-bin-size|-b <bin-size>] \\\n"
            "        [--histogram <histogram-path>]\n",
            args->executable);
    // Help options
    fprintf(stream, "\nHelp Options:\n");
    fprintf(stream,
            "    --help, -h: print this help message. Overrides all else!\n");
    // Application options
    fprintf(stream, "\nApplication Options:\n");
    fprintf(stream,
            "    --input, -i <input-path>: path to the input ('~/...' may not "
            "work) or 'zipf' (for a randomly generated Zipfian distribution) "
            "or 'step' (for a step function) or 'two-step' (for two steps) or "
            "'two-distr' (for two distributions)\n");
    fprintf(stream,
            "    --format, -f <input-trace-format>: format for the input "
            "trace, pick ");
    print_available_trace_formats(stream);
    fprintf(stream, "\n");
    fprintf(stream, "    --algorithm, -a <algorithm>: algorithm, pick ");
    print_available_algorithms(stream);
    fprintf(stream, "\n");
    fprintf(stream,
            "    --output, -o <output-path>: path to the output file ('~/...' "
            "may not work)\n");
    fprintf(stream,
            "    --sampling-ratio, -s <ratio in (0.0, 1.0]>: ratio of for "
            "SHARDS (must pick a SHARDS algorithm). Default: %g.\n",
            DEFAULT_SHARDS_SAMPLING_RATIO);
    fprintf(stream,
            "    --number-entries, -n <trace-length>: number of entries in an "
            "artificial trace (must pick an artificial trace, e.g. 'zipf'). "
            "Default: %zu.\n",
            DEFAULT_ARTIFICIAL_TRACE_LENGTH);
    fprintf(stream,
            "    --oracle: the oracle path to use as a cache for the Olken "
            "results. Default: %s.\n",
            DEFAULT_ORACLE_PATH ? DEFAULT_ORACLE_PATH : "(null)");
    fprintf(stream,
            "    --hist-bin-size, -b <bin-size>: the histogram bin size. "
            "Default: %zu.\n",
            DEFAULT_HIST_BIN_SIZE);
    fprintf(
        stream,
        "    --histogram <histogram-output-path>: path to save the histogram. "
        "Default: %s.\n",
        DEFAULT_HISTOGRAM_PATH ? DEFAULT_ORACLE_PATH : "(null)");
    fprintf(stream,
            "N.B. '~/path/to/file' paths are not guaranteed to work. Use "
            "relative (e.g. '../path/to/file' or './path/to/file') or absolute "
            "paths (e.g. '/path/to/file')\n");
}

static inline bool
matches_option(char *arg, char *long_option, char *short_option)
{
    return strcmp(arg, long_option) == 0 || strcmp(arg, short_option) == 0;
}

static inline size_t
parse_positive_size(char const *const str)
{
    char *endptr = NULL;
    unsigned long long u = strtoull(str, &endptr, 10);
    if (u == ULLONG_MAX && errno == ERANGE) {
        LOGGER_ERROR("integer (%s) out of range", str);
        exit(EXIT_FAILURE);
    }
    if (&str[strlen(str)] != endptr) {
        LOGGER_ERROR("only the first %d characters of '%s' was interpreted",
                     endptr - str,
                     str);
        exit(EXIT_FAILURE);
    }
    // I'm assuming the conversion for ULL to size_t is safe...
    return (size_t)u;
}

static inline double
parse_positive_double(char const *const str)
{
    char *endptr = NULL;
    double d = strtod(str, &endptr);
    if (d < 0.0 || (d == HUGE_VAL && errno == ERANGE)) {
        LOGGER_ERROR("number (%s) out of range", str);
        exit(EXIT_FAILURE);
    }
    if (&str[strlen(str)] != endptr) {
        LOGGER_ERROR("only the first %d characters of '%s' was interpreted",
                     endptr - str,
                     str);
        exit(EXIT_FAILURE);
    }
    return d;
}

static void
print_command_line_arguments(struct CommandLineArguments const *args)
{
    fprintf(
        stderr,
        "CommandLineArguments(executable='%s', input_path='%s', "
        "algorithm='%s', output_path='%s', shards_ratio='%g', "
        "artifical_trace_length='%zu', oracle_path='%s', hist_bin_size=%zu)\n",
        args->executable,
        args->input_path,
        algorithm_names[args->algorithm],
        args->output_path,
        args->shards_sampling_ratio,
        args->artificial_trace_length,
        args->oracle_path ? args->oracle_path : "(null)",
        args->hist_bin_size);
}

static void
print_trace_summary(struct CommandLineArguments const *args,
                    struct Trace const *const trace)
{
    fprintf(stderr,
            "Trace(source='%s', format='%s', length=%zu)\n",
            args->input_path,
            TRACE_FORMAT_STRINGS[args->trace_format],
            trace->length);
}

static enum MRCAlgorithm
parse_algorithm_string(struct CommandLineArguments const *args, char *str)
{
    for (size_t i = 1; i < ARRAY_SIZE(algorithm_names); ++i) {
        if (strcmp(algorithm_names[i], str) == 0)
            return (enum MRCAlgorithm)i;
    }

    LOGGER_ERROR("unparsable algorithm string: '%s'", str);
    fprintf(LOGGER_STREAM, "   expected: ");
    print_available_algorithms(LOGGER_STREAM);
    fprintf(LOGGER_STREAM, "\n");
    print_help(stdout, args);
    exit(-1);
}

static struct CommandLineArguments
parse_command_line_arguments(int argc, char **argv)
{
    struct CommandLineArguments args = {0};
    args.executable = argv[0];

    // Set defaults
    args.shards_sampling_ratio = DEFAULT_SHARDS_SAMPLING_RATIO;
    args.artificial_trace_length = DEFAULT_ARTIFICIAL_TRACE_LENGTH;
    args.hist_bin_size = DEFAULT_HIST_BIN_SIZE;

    // Set parameters based on user arguments
    for (int i = 1; i < argc; ++i) {
        if (matches_option(argv[i], "--input", "-i")) {
            ++i;
            if (i >= argc) {
                LOGGER_ERROR(
                    "expecting input path (or 'zipf', 'step', 'two-step')");
                print_help(stdout, &args);
                exit(-1);
            }
            args.input_path = argv[i];
        } else if (matches_option(argv[i], "--format", "-f")) {
            ++i;
            if (i >= argc) {
                LOGGER_ERROR(
                    "expecting input path (or 'zipf, 'step', 'two-step'')");
                print_help(stdout, &args);
                exit(-1);
            }
            args.trace_format = parse_trace_format_string(argv[i]);
            if (args.trace_format == TRACE_FORMAT_INVALID) {
                print_help(stdout, &args);
                exit(-1);
            }
        } else if (matches_option(argv[i], "--algorithm", "-a")) {
            ++i;
            if (i >= argc) {
                LOGGER_ERROR("expecting algorithm!");
                print_help(stdout, &args);
                exit(-1);
            }
            args.algorithm = parse_algorithm_string(&args, argv[i]);
        } else if (matches_option(argv[i], "--output", "-o")) {
            ++i;
            if (i >= argc) {
                LOGGER_ERROR("expecting output path!");
                print_help(stdout, &args);
                exit(-1);
            }
            args.output_path = argv[i];
        } else if (matches_option(argv[i], "--sampling-ratio", "-s")) {
            ++i;
            if (i >= argc) {
                LOGGER_ERROR("expecting sampling ratio!");
                print_help(stdout, &args);
                exit(-1);
            }
            args.shards_sampling_ratio = parse_positive_double(argv[i]);
        } else if (matches_option(argv[i], "--number-entries", "-n")) {
            ++i;
            if (i >= argc) {
                LOGGER_ERROR("expecting output path!");
                print_help(stdout, &args);
                exit(-1);
            }
            args.artificial_trace_length = parse_positive_size(argv[i]);
        } else if (matches_option(argv[i], "--oracle", "--oracle")) {
            // NOTE There is no short form of the oracle flag.
            ++i;
            if (i >= argc) {
                LOGGER_ERROR("expecting oracle path!");
                print_help(stdout, &args);
                exit(-1);
            }
            args.oracle_path = argv[i];
        } else if (matches_option(argv[i], "--hist-bin-size", "-b")) {
            ++i;
            if (i >= argc) {
                LOGGER_ERROR("expecting histogram bin size!");
                print_help(stdout, &args);
                exit(-1);
            }
            args.hist_bin_size = parse_positive_size(argv[i]);
        } else if (matches_option(argv[i], "--histogram", "--histogram")) {
            ++i;
            if (i >= argc) {
                LOGGER_ERROR("expecting histogram output path!");
                print_help(stdout, &args);
                exit(-1);
            }
            args.hist_output_path = argv[i];
        } else if (matches_option(argv[i], "--help", "-h")) {
            print_help(stdout, &args);
            exit(0);
        } else {
            // TODO(dchu): print out the unexpected argument that we received
            LOGGER_ERROR("unexpected argument: '%s'!", argv[i]);
            print_help(stdout, &args);
            exit(-1);
        }
    }

    // Check existence of required arguments
    bool error = false;
    if (args.input_path == NULL) {
        LOGGER_ERROR("must specify input path!");
        error = true;
    }
    if (args.trace_format == TRACE_FORMAT_INVALID &&
        strcmp(args.input_path, "zipf") != 0 &&
        strcmp(args.input_path, "step") &&
        strcmp(args.input_path, "two-step")) {
        LOGGER_WARN("trace format was not specified, so defaulting to Kia's");
        args.trace_format = TRACE_FORMAT_KIA;
    }
    if (args.algorithm == MRC_ALGORITHM_INVALID) {
        LOGGER_ERROR("must specify algorithm!");
        error = true;
    }
    if (args.output_path == NULL) {
        LOGGER_ERROR("must specify output path!");
        error = true;
    }
    if (error)
        goto cleanup;

    return args;

cleanup:
    print_help(stdout, &args);
    exit(-1);
}

/// @note   I specify the var_name and args_name explicitly because the init
///         function needs to use these names, so I don't want the user to be
///         confused about magic variables. Maybe this is more confusing.
#define CONSTRUCT_RUN_ALGORITHM_FUNCTION(func_name,                            \
                                         type,                                 \
                                         var_name,                             \
                                         args_name,                            \
                                         init_call,                            \
                                         access_func,                          \
                                         post_process_func,                    \
                                         hist,                                 \
                                         save_hist_func,                       \
                                         to_mrc_func,                          \
                                         destroy_func)                         \
    static struct MissRateCurve func_name(                                     \
        struct Trace const *const trace,                                       \
        struct CommandLineArguments const args_name)                           \
    {                                                                          \
        MAYBE_UNUSED(args_name);                                               \
        type var_name = {0};                                                   \
        LOGGER_TRACE("Initialize MRC Algorithm");                              \
        g_assert_true((init_call));                                            \
        LOGGER_TRACE("Begin running trace");                                   \
        double t0 = get_wall_time_sec();                                       \
        for (size_t i = 0; i < trace->length; ++i) {                           \
            ((access_func))(&var_name, trace->trace[i].key);                   \
            if (i % 1000000 == 0) {                                            \
                LOGGER_TRACE("Finished %zu / %zu", i, trace->length);          \
            }                                                                  \
        }                                                                      \
        double t1 = get_wall_time_sec();                                       \
        LOGGER_TRACE("Begin post process");                                    \
        ((post_process_func))(&var_name);                                      \
        double t2 = get_wall_time_sec();                                       \
        struct MissRateCurve mrc = {0};                                        \
        ((to_mrc_func))(&var_name, &mrc);                                      \
        double t3 = get_wall_time_sec();                                       \
        LOGGER_INFO("Histogram Time: %f | Post-Process Time: %f | MRC Time: "  \
                    "%f | Total Time: %f",                                     \
                    (double)(t1 - t0),                                         \
                    (double)(t2 - t1),                                         \
                    (double)(t3 - t2),                                         \
                    (double)(t3 - t0));                                        \
        if (args.hist_output_path != NULL) {                                   \
            ((save_hist_func))(hist, args.hist_output_path);                   \
            LOGGER_TRACE("Wrote histogram");                                   \
        }                                                                      \
        ((destroy_func))(&var_name);                                           \
        LOGGER_TRACE("Destroyed MRC generator object");                        \
        return mrc;                                                            \
    }

CONSTRUCT_RUN_ALGORITHM_FUNCTION(run_olken,
                                 struct Olken,
                                 me,
                                 args,
                                 Olken__init(&me,
                                             trace->length,
                                             args.hist_bin_size),
                                 Olken__access_item,
                                 Olken__post_process,
                                 &me.histogram,
                                 Histogram__save_sparse,
                                 Olken__to_mrc,
                                 Olken__destroy)

CONSTRUCT_RUN_ALGORITHM_FUNCTION(
    run_fixed_rate_shards,
    struct FixedRateShards,
    me,
    args,
    FixedRateShards__init(&me,
                          args.shards_sampling_ratio,
                          trace->length,
                          args.hist_bin_size,
                          false),
    FixedRateShards__access_item,
    FixedRateShards__post_process,
    &me.olken.histogram,
    Histogram__save_sparse,
    FixedRateShards__to_mrc,
    FixedRateShards__destroy)

CONSTRUCT_RUN_ALGORITHM_FUNCTION(
    run_fixed_rate_shards_adj,
    struct FixedRateShards,
    me,
    args,
    FixedRateShards__init(&me,
                          args.shards_sampling_ratio,
                          trace->length,
                          args.hist_bin_size,
                          true),
    FixedRateShards__access_item,
    FixedRateShards__post_process,
    &me.olken.histogram,
    Histogram__save_sparse,
    FixedRateShards__to_mrc,
    FixedRateShards__destroy)

CONSTRUCT_RUN_ALGORITHM_FUNCTION(
    run_fixed_size_shards,
    struct FixedSizeShards,
    me,
    args,
    FixedSizeShards__init(&me,
                          args.shards_sampling_ratio,
                          1 << 13,
                          trace->length,
                          args.hist_bin_size),
    FixedSizeShards__access_item,
    FixedSizeShards__post_process,
    &me.histogram,
    Histogram__save_sparse,
    FixedSizeShards__to_mrc,
    FixedSizeShards__destroy)

CONSTRUCT_RUN_ALGORITHM_FUNCTION(run_quickmrc,
                                 struct QuickMRC,
                                 me,
                                 args,
                                 QuickMRC__init(&me,
                                                args.shards_sampling_ratio,
                                                1024,
                                                1 << 8,
                                                trace->length,
                                                args.hist_bin_size),
                                 QuickMRC__access_item,
                                 QuickMRC__post_process,
                                 &me.histogram,
                                 Histogram__save_sparse,
                                 QuickMRC__to_mrc,
                                 QuickMRC__destroy)

CONSTRUCT_RUN_ALGORITHM_FUNCTION(run_goel_quickmrc,
                                 struct GoelQuickMRC,
                                 me,
                                 args,
                                 // Use the same configuration as Ashvin
                                 GoelQuickMRC__init(&me,
                                                    args.shards_sampling_ratio,
                                                    trace->length,
                                                    10,
                                                    7,
                                                    0,
                                                    true),
                                 GoelQuickMRC__access_item,
                                 GoelQuickMRC__post_process,
                                 &me,
                                 GoelQuickMRC__save_sparse_histogram,
                                 GoelQuickMRC__to_mrc,
                                 GoelQuickMRC__destroy)

CONSTRUCT_RUN_ALGORITHM_FUNCTION(
    run_evicting_map,
    struct EvictingMap,
    me,
    args,
    BucketedShards__init(&me,
                         args.shards_sampling_ratio,
                         1 << 13,
                         trace->length,
                         args.hist_bin_size),
    BucketedShards__access_item,
    BucketedShards__post_process,
    &me.histogram,
    Histogram__save_sparse,
    EvictingMap__to_mrc,
    BucketedShards__destroy)

CONSTRUCT_RUN_ALGORITHM_FUNCTION(run_average_eviction_time,
                                 struct AverageEvictionTime,
                                 me,
                                 args,
                                 AverageEvictionTime__init(&me,
                                                           trace->length,
                                                           args.hist_bin_size,
                                                           trace->length / 100),
                                 AverageEvictionTime__access_item,
                                 AverageEvictionTime__post_process,
                                 &me.histogram,
                                 Histogram__save_sparse,
                                 AverageEvictionTime__to_mrc,
                                 AverageEvictionTime__destroy)

CONSTRUCT_RUN_ALGORITHM_FUNCTION(
    run_their_average_eviction_time,
    struct AverageEvictionTime,
    me,
    args,
    AverageEvictionTime__init(&me, trace->length, args.hist_bin_size, false),
    AverageEvictionTime__access_item,
    AverageEvictionTime__post_process,
    &me.histogram,
    Histogram__save_sparse,
    AverageEvictionTime__their_to_mrc,
    AverageEvictionTime__destroy)

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

static struct MissRateCurve
get_oracle_mrc(struct CommandLineArguments const args,
               struct Trace const *const trace,
               struct MissRateCurve const *const mrc)
{
    bool r = false;
    struct MissRateCurve oracle_mrc = {0};
    if (file_exists(args.oracle_path)) {
        LOGGER_TRACE("using existing oracle");
        r = MissRateCurve__init_from_sparse_file(&oracle_mrc,
                                                 args.oracle_path,
                                                 trace->length,
                                                 1);
        g_assert_true(r);
        return oracle_mrc;
    } else if (args.algorithm == MRC_ALGORITHM_OLKEN) {
        LOGGER_TRACE("using Olken result as oracle");
        r = MissRateCurve__write_sparse_binary_to_file(mrc, args.oracle_path);
        g_assert_true(r);
        r = MissRateCurve__init_from_sparse_file(&oracle_mrc,
                                                 args.oracle_path,
                                                 mrc->num_bins,
                                                 mrc->bin_size);
        g_assert_true(r);
        return oracle_mrc;
    } else {
        LOGGER_TRACE("running Olken to produce oracle");
        oracle_mrc = run_olken(trace, args);
        r = MissRateCurve__write_sparse_binary_to_file(&oracle_mrc,
                                                       args.oracle_path);
        g_assert_true(r);
        return oracle_mrc;
    }
}

int
main(int argc, char **argv)
{
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

    struct MissRateCurve mrc = {0};
    switch (args.algorithm) {
    case MRC_ALGORITHM_OLKEN:
        LOGGER_TRACE("running Olken");
        mrc = run_olken(&trace, args);
        break;
    case MRC_ALGORITHM_FIXED_RATE_SHARDS:
        LOGGER_TRACE("running Fixed-Rate SHARDS");
        mrc = run_fixed_rate_shards(&trace, args);
        break;
    case MRC_ALGORITHM_FIXED_RATE_SHARDS_ADJ:
        LOGGER_TRACE("running Fixed-Rate SHARDS Adjusted");
        mrc = run_fixed_rate_shards_adj(&trace, args);
        break;
    case MRC_ALGORITHM_FIXED_SIZE_SHARDS:
        LOGGER_TRACE("running Fixed-Size SHARDS");
        mrc = run_fixed_size_shards(&trace, args);
        break;
    case MRC_ALGORITHM_QUICKMRC:
        LOGGER_TRACE("running QuickMRC");
        mrc = run_quickmrc(&trace, args);
        break;
    case MRC_ALGORITHM_GOEL_QUICKMRC:
        LOGGER_TRACE("running Ashvin Goel's QuickMRC");
        mrc = run_goel_quickmrc(&trace, args);
        break;
    case MRC_ALGORITHM_EVICTING_MAP:
        LOGGER_TRACE("running Evicting Map");
        mrc = run_evicting_map(&trace, args);
        break;
    case MRC_ALGORITHM_AVERAGE_EVICTION_TIME:
        LOGGER_TRACE("running Average Eviction Time");
        mrc = run_average_eviction_time(&trace, args);
        break;
    case MRC_ALGORITHM_THEIR_AVERAGE_EVICTION_TIME:
        LOGGER_TRACE("running author's pseudocode Average Eviction Time");
        mrc = run_their_average_eviction_time(&trace, args);
        break;
    default:
        LOGGER_ERROR("invalid algorithm %d", args.algorithm);
        return EXIT_FAILURE;
    }

    // Optionally check MAE and MSE
    if (args.oracle_path != NULL) {
        LOGGER_TRACE("Comparing against oracle");
        struct MissRateCurve oracle_mrc = {0};
        oracle_mrc = get_oracle_mrc(args, &trace, &mrc);

        double mse = MissRateCurve__mean_squared_error(&oracle_mrc, &mrc);
        double mae = MissRateCurve__mean_absolute_error(&oracle_mrc, &mrc);
        LOGGER_INFO("Mean Squared Error: %f", mse);
        LOGGER_INFO("Mean Absolute Error: %f", mae);

        MissRateCurve__destroy(&oracle_mrc);
    }

    // Write out trace
    g_assert_true(
        MissRateCurve__write_sparse_binary_to_file(&mrc, args.output_path));
    LOGGER_TRACE("Wrote out sparse MRC to '%s'", args.output_path);
    MissRateCurve__destroy(&mrc);
    LOGGER_TRACE("Destroyed MRC object");
    Trace__destroy(&trace);
    LOGGER_TRACE("Destroyed trace object");

    return 0;
}
