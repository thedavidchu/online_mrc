#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "arrays/array_size.h"
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

static const size_t DEFAULT_TRACE_LENGTH = 1 << 28;
static const double DEFAULT_SHARDS_SAMPLING_RATIO = 1e-3;

enum MRCAlgorithm {
    MRC_ALGORITHM_INVALID,
    MRC_ALGORITHM_OLKEN,
    MRC_ALGORITHM_FIXED_RATE_SHARDS,
    MRC_ALGORITHM_FIXED_RATE_SHARDS_ADJ,
    MRC_ALGORITHM_FIXED_SIZE_SHARDS,
    MRC_ALGORITHM_QUICKMRC,
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
};

struct CommandLineArguments {
    char *executable;
    enum MRCAlgorithm algorithm;
    char *input_path;
    char *output_path;

    double shards_sampling_ratio;
    size_t trace_length;

    /* UNUSED */
    int size;
    int repeats;
    bool check;
    bool debug;
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

static void
print_help(FILE *stream, struct CommandLineArguments const *args)
{
    fprintf(stream,
            "Usage: %s [--input|-i <input-path>] [--algorithm|-a <algorithm>] "
            "[--output|-o <output-path>]\n",
            args->executable);
    fprintf(
        stream,
        "    --input, -i <input-path>: path to the input ('~/...' may not "
        "work) or 'zipf' (for a randomly generated Zipfian distribution)\n");
    fprintf(stream, "    --algorithm, -a <algorithm>: algorithm, pick ");
    print_available_algorithms(stream);
    fprintf(stream, "\n");
    fprintf(stream,
            "    --output, -o <output-path>: path to the output file ('~/...' "
            "may not work)\n");
    fprintf(stream,
            "    --sampling-ratio, -s <ratio in (0.0, 1.0]>: ratio of for "
            "SHARDS (must pick a SHARDS algorithm). Default: %g.",
            DEFAULT_SHARDS_SAMPLING_RATIO);
    fprintf(stream,
            "    --number-entries, -n <trace-length>: number of entries in an "
            "artificial trace (must pick an artificial trace, e.g. 'zipf'). "
            "Default: %zu.",
            DEFAULT_TRACE_LENGTH);
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

static inline char *
bool_to_string(bool x)
{
    return x ? "true" : "false";
}

static void
print_command_line_arguments(struct CommandLineArguments const *args)
{
    printf("CommandLineArguments(executable='%s', input='%s', algorithm='%s', "
           "output='%s')\n",
           args->executable,
           args->input_path,
           algorithm_names[args->algorithm],
           args->output_path);
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
    args.trace_length = DEFAULT_TRACE_LENGTH;

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
            args.trace_length = parse_positive_size(argv[i]);
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
        struct Trace *trace,                                                   \
        struct CommandLineArguments args_name)                                 \
    {                                                                          \
        MAYBE_UNUSED(args_name);                                               \
        type var_name = {0};                                                   \
        g_assert_true((init_call));                                            \
        clock_t t0 = clock();                                                  \
        for (size_t i = 0; i < trace->length; ++i) {                           \
            ((access_func))(&var_name, trace->trace[i].key);                   \
        }                                                                      \
        clock_t t1 = clock();                                                  \
        ((post_process_func))(&var_name);                                      \
        clock_t t2 = clock();                                                  \
        LOGGER_INFO("Runtime: %f, Post-Process Time: %f, Total Time: %f",      \
                    (double)(t1 - t0) / CLOCKS_PER_SEC,                        \
                    (double)(t2 - t1) / CLOCKS_PER_SEC,                        \
                    (double)(t2 - t0) / CLOCKS_PER_SEC);                       \
        struct MissRateCurve mrc = {0};                                        \
        if (false) {                                                           \
            Histogram__write_as_json(stdout, &var_name.hist);                  \
        }                                                                      \
        ((hist_func))(&mrc, &var_name.hist);                                   \
        ((destroy_func))(&var_name);                                           \
        return mrc;                                                            \
    }

CONSTRUCT_RUN_ALGORITHM_FUNCTION(run_olken,
                                 struct Olken,
                                 me,
                                 args,
                                 Olken__init(&me, trace->length, 1),
                                 Olken__access_item,
                                 Olken__post_process,
                                 histogram,
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
    olken.histogram,
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
    olken.histogram,
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
    histogram,
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
                                 histogram,
                                 MissRateCurve__init_from_histogram,
                                 QuickMRC__destroy)

/// @note   I introduce this function so that I can do perform some logic but
///         also maintain the constant-qualification of the members of struct
///         Trace.
static struct Trace
get_trace(struct CommandLineArguments args)
{
    if (strcmp(args.input_path, "zipf") == 0) {
        return generate_trace(args.trace_length, args.trace_length, 0.99, 0);
    } else {
        return read_trace(args.input_path);
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
        mrc = run_olken(&trace, args);
        break;
    case MRC_ALGORITHM_FIXED_RATE_SHARDS:
        mrc = run_fixed_rate_shards(&trace, args);
        break;
    case MRC_ALGORITHM_FIXED_RATE_SHARDS_ADJ:
        mrc = run_fixed_rate_shards_adj(&trace, args);
        break;
    case MRC_ALGORITHM_FIXED_SIZE_SHARDS:
        mrc = run_fixed_size_shards(&trace, args);
        break;
    case MRC_ALGORITHM_QUICKMRC:
        mrc = run_quickmrc(&trace, args);
        break;
    default:
        LOGGER_ERROR("invalid algorithm %d", args.algorithm);
        return EXIT_FAILURE;
    }

    // Write out trace
    g_assert_true(MissRateCurve__write_binary_to_file(&mrc, args.output_path));
    MissRateCurve__destroy(&mrc);
    Trace__destroy(&trace);

    return 0;
}