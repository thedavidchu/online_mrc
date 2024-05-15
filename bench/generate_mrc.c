#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "arrays/array_size.h"
#include "bucketed_shards/bucketed_shards.h"
#include "goel_quickmrc/goel_quickmrc.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "quickmrc/quickmrc.h"
#include "shards/fixed_rate_shards.h"
#include "shards/fixed_size_shards.h"
#include "trace/generator.h"
#include "trace/reader.h"
#include "trace/trace.h"
#include "unused/mark_unused.h"

static const size_t DEFAULT_ARTIFICIAL_TRACE_LENGTH = 1 << 20;
static const double DEFAULT_SHARDS_SAMPLING_RATIO = 1e-3;
static char *DEFAULT_ORACLE_PATH = NULL;

static char const *const BOOLEAN_STRINGS[2] = {"false", "true"};

enum MRCAlgorithm {
    MRC_ALGORITHM_INVALID,
    MRC_ALGORITHM_OLKEN,
    MRC_ALGORITHM_FIXED_RATE_SHARDS,
    MRC_ALGORITHM_FIXED_RATE_SHARDS_ADJ,
    MRC_ALGORITHM_FIXED_SIZE_SHARDS,
    MRC_ALGORITHM_QUICKMRC,
    MRC_ALGORITHM_GOEL_QUICKMRC,
    MRC_ALGORITHM_BUCKETED_SHARDS,
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
    "Bucketed-SHARDS",
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
};

static void
print_available_trace_formats(FILE *stream)
{
    fprintf(stream, "{");
    // NOTE We want to skip the "INVALID" algorithm name (i.e. 0).
    for (size_t i = 1; i < ARRAY_SIZE(TRACE_FORMAT_STRINGS); ++i) {
        fprintf(stream, "%s", TRACE_FORMAT_STRINGS[i]);
        if (i != ARRAY_SIZE(TRACE_FORMAT_STRINGS) - 1) {
            fprintf(stream, ",");
        }
    }
    fprintf(stream, "}");
}

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
            "Usage: %s --input|-i <input-path> --algorithm|-a <algorithm> "
            "--output|-o <output-path> [--sampling-ratio|-s <ratio>] "
            "[--number-entries|-n <trace-length>] [--oracle <oracle-path>]\n",
            args->executable);
    fprintf(
        stream,
        "    --input, -i <input-path>: path to the input ('~/...' may not "
        "work) or 'zipf' (for a randomly generated Zipfian distribution)\n");
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
            "    --help, -h: print this help message. Overrides all else!\n");
}

static inline bool
matches_option(char *arg, char *long_option, char *short_option)
{
    return strcmp(arg, long_option) == 0 || strcmp(arg, short_option) == 0;
}

static inline size_t
parse_positive_size(char const *const str)
{
    unsigned long long u = strtoull(str, NULL, 10);
    assert(!(u == ULLONG_MAX && errno == ERANGE));
    // I'm assuming the conversion for ULL to size_t is safe...
    return (size_t)u;
}

static inline double
parse_positive_double(char const *const str)
{
    double d = strtod(str, NULL);
    assert(d > 0.0 && !(d == HUGE_VAL && errno == ERANGE));
    return d;
}

static void
print_command_line_arguments(struct CommandLineArguments const *args)
{
    printf("CommandLineArguments(executable='%s', input_path='%s', "
           "algorithm='%s', output_path='%s', shards_ratio='%g', "
           "artifical_trace_length='%zu', oracle_path='%s')\n",
           args->executable,
           args->input_path,
           algorithm_names[args->algorithm],
           args->output_path,
           args->shards_sampling_ratio,
           args->artificial_trace_length,
           args->oracle_path ? args->oracle_path : "(null)");
}

static enum TraceFormat
parse_input_format_string(struct CommandLineArguments const *args, char *str)
{
    for (size_t i = 1; i < ARRAY_SIZE(TRACE_FORMAT_STRINGS); ++i) {
        if (strcmp(TRACE_FORMAT_STRINGS[i], str) == 0)
            return (enum TraceFormat)i;
    }
    LOGGER_ERROR("unparsable format string: '%s'", str);
    fprintf(LOGGER_STREAM, "   expected: ");
    print_available_trace_formats(LOGGER_STREAM);
    fprintf(LOGGER_STREAM, "\n");
    print_help(stdout, args);
    exit(-1);
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

    // Set parameters based on user arguments
    for (int i = 1; i < argc; ++i) {
        if (matches_option(argv[i], "--input", "-i")) {
            ++i;
            if (i >= argc) {
                LOGGER_ERROR("expecting input path (or 'zipf')");
                print_help(stdout, &args);
                exit(-1);
            }
            args.input_path = argv[i];
        } else if (matches_option(argv[i], "--format", "-f")) {
            ++i;
            if (i >= argc) {
                LOGGER_ERROR("expecting input path (or 'zipf')");
                print_help(stdout, &args);
                exit(-1);
            }
            args.trace_format = parse_input_format_string(&args, argv[i]);
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
        strcmp(args.input_path, "zipf") != 0) {
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

    print_command_line_arguments(&args);

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
                                         hist_func,                            \
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
        clock_t t0 = clock();                                                  \
        for (size_t i = 0; i < trace->length; ++i) {                           \
            ((access_func))(&var_name, trace->trace[i].key);                   \
            if (i % 1000000 == 0) {                                            \
                LOGGER_TRACE("Finished %zu / %zu", i, trace->length);          \
            }                                                                  \
        }                                                                      \
        clock_t t1 = clock();                                                  \
        LOGGER_TRACE("Begin post process");                                    \
        ((post_process_func))(&var_name);                                      \
        clock_t t2 = clock();                                                  \
        LOGGER_INFO("Runtime: %f | Post-Process Time: %f | Total Time: %f",    \
                    (double)(t1 - t0) / CLOCKS_PER_SEC,                        \
                    (double)(t2 - t1) / CLOCKS_PER_SEC,                        \
                    (double)(t2 - t0) / CLOCKS_PER_SEC);                       \
        struct MissRateCurve mrc = {0};                                        \
        ((hist_func))(&mrc, hist);                                             \
        LOGGER_TRACE("Wrote histogram");                                       \
        ((destroy_func))(&var_name);                                           \
        LOGGER_TRACE("Destroyed MRC generator object");                        \
        return mrc;                                                            \
    }

CONSTRUCT_RUN_ALGORITHM_FUNCTION(run_olken,
                                 struct Olken,
                                 me,
                                 args,
                                 Olken__init(&me, trace->length, 1),
                                 Olken__access_item,
                                 Olken__post_process,
                                 &me.histogram,
                                 MissRateCurve__init_from_histogram,
                                 Olken__destroy)

CONSTRUCT_RUN_ALGORITHM_FUNCTION(
    run_fixed_rate_shards,
    struct FixedRateShards,
    me,
    args,
    FixedRateShards__init(&me,
                          trace->length,
                          args.shards_sampling_ratio,
                          1,
                          false),
    FixedRateShards__access_item,
    FixedRateShards__post_process,
    &me.olken.histogram,
    MissRateCurve__init_from_histogram,
    FixedRateShards__destroy)

CONSTRUCT_RUN_ALGORITHM_FUNCTION(
    run_fixed_rate_shards_adj,
    struct FixedRateShards,
    me,
    args,
    FixedRateShards__init(&me,
                          trace->length,
                          args.shards_sampling_ratio,
                          1,
                          true),
    FixedRateShards__access_item,
    FixedRateShards__post_process,
    &me.olken.histogram,
    MissRateCurve__init_from_histogram,
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
                          1),
    FixedSizeShards__access_item,
    FixedSizeShards__post_process,
    &me.histogram,
    MissRateCurve__init_from_histogram,
    FixedSizeShards__destroy)

CONSTRUCT_RUN_ALGORITHM_FUNCTION(run_quickmrc,
                                 struct QuickMRC,
                                 me,
                                 args,
                                 QuickMRC__init(&me,
                                                1024,
                                                1 << 8,
                                                trace->length,
                                                args.shards_sampling_ratio),
                                 QuickMRC__access_item,
                                 QuickMRC__post_process,
                                 &me.histogram,
                                 MissRateCurve__init_from_histogram,
                                 QuickMRC__destroy)

CONSTRUCT_RUN_ALGORITHM_FUNCTION(run_goel_quickmrc,
                                 struct GoelQuickMRC,
                                 me,
                                 args,
                                 // Use the same configuration as Ashvin
                                 GoelQuickMRC__init(&me,
                                                    trace->length,
                                                    10,
                                                    7,
                                                    0,
                                                    args.shards_sampling_ratio),
                                 GoelQuickMRC__access_item,
                                 GoelQuickMRC__post_process,
                                 &me,
                                 GoelQuickMRC__to_mrc,
                                 GoelQuickMRC__destroy)

CONSTRUCT_RUN_ALGORITHM_FUNCTION(
    run_bucketed_shards,
    struct BucketedShards,
    me,
    args,
    BucketedShards__init(&me,
                         1 << 13,
                         trace->length,
                         args.shards_sampling_ratio,
                         1),
    BucketedShards__access_item,
    BucketedShards__post_process,
    &me.histogram,
    MissRateCurve__init_from_histogram,
    BucketedShards__destroy)
/// @note   I introduce this function so that I can do perform some logic but
///         also maintain the constant-qualification of the members of struct
///         Trace.
static struct Trace
get_trace(struct CommandLineArguments args)
{
    if (strcmp(args.input_path, "zipf") == 0) {
        LOGGER_TRACE("Generating artificial Zipfian trace");
        return generate_trace(args.artificial_trace_length,
                              args.artificial_trace_length,
                              0.99,
                              0);
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
    struct MissRateCurve oracle_mrc = {0};
    bool oracle_exists = MissRateCurve__init_from_file(&oracle_mrc,
                                                       args.oracle_path,
                                                       trace->length,
                                                       1);
    if (oracle_exists) {
        LOGGER_TRACE("using existing oracle");
        return oracle_mrc;
    } else if (args.algorithm == MRC_ALGORITHM_OLKEN) {
        LOGGER_TRACE("using Olken result as oracle");
        g_assert_true(
            MissRateCurve__write_binary_to_file(mrc, args.oracle_path));
        g_assert_true(MissRateCurve__init_from_file(&oracle_mrc,
                                                    args.oracle_path,
                                                    mrc->num_bins,
                                                    mrc->bin_size));
        return oracle_mrc;
    } else {
        LOGGER_TRACE("running Olken to produce oracle");
        LOGGER_WARN("running Olken to produce oracle");
        oracle_mrc = run_olken(trace, args);
        g_assert_true(
            MissRateCurve__write_binary_to_file(&oracle_mrc, args.oracle_path));
        g_assert_true(MissRateCurve__init_from_file(&oracle_mrc,
                                                    args.oracle_path,
                                                    mrc->num_bins,
                                                    mrc->bin_size));
        return oracle_mrc;
    }
}

int
main(int argc, char **argv)
{
    struct CommandLineArguments args = {0};
    args = parse_command_line_arguments(argc, argv);

    // Read in trace
    struct Trace trace = get_trace(args);
    if (trace.trace == NULL || trace.length == 0) {
        // I cast to (void *) so that it doesn't complain about printing it.
        LOGGER_ERROR("invalid trace {.trace = %p, .length = %zu}",
                     (void *)trace.trace,
                     trace.length);
        return EXIT_FAILURE;
    }

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
    case MRC_ALGORITHM_BUCKETED_SHARDS:
        LOGGER_TRACE("running Bucketed Shards");
        mrc = run_bucketed_shards(&trace, args);
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
