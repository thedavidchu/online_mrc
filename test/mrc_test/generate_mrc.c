#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "shards/fixed_rate_shards.h"
#include "trace/reader.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum MRCAlgorithm {
    MRC_ALGORITHM_INVALID,
    MRC_ALGORITHM_OLKEN,
    MRC_ALGORITHM_FIXED_RATE_SHARDS,
};

// NOTE This corresponds to the same order as MRCAlgorithm so that we can
//      simply use the enumeration to print the correct string!
static char *algorithm_names[] = {
    "INVALID",
    "Olken",
    "Fixed-Rate-SHARDS",
};

struct CommandLineArguments {
    char *executable;
    enum MRCAlgorithm algorithm;
    char *input_path;
    char *output_path;

    /* UNUSED */
    int size;
    int repeats;
    bool check;
    bool debug;
};

static void
print_help(FILE *stream, struct CommandLineArguments const *args)
{
    fprintf(stream,
            "Usage: %s [--input|-i <input-path>] [--algorithm|-a <algorithm>] "
            "[--output|-o <output-path>]\n",
            args->executable);
    fprintf(stream,
            "    --input, -i <input-path>: path to the input ('~/...' may not "
            "work)\n");
    fprintf(stream,
            "    --algorithm, -a <algorithm>: algorithm, pick "
            "{Olken,Fixed-Rate-SHARDS}\n");
    fprintf(stream,
            "    --output, -o <output-path>: path to the output file ('~/...' "
            "may not work)\n");
    fprintf(stream,
            "    --help, -h: print this help message. Overrides all else!\n");
}

static inline bool
matches_option(char *arg, char *long_option, char *short_option)
{
    return strcmp(arg, long_option) == 0 || strcmp(arg, short_option) == 0;
}

static inline char *
bool_to_string(bool x)
{
    return x ? "true" : "false";
}

static void
print_command_line_arguments(struct CommandLineArguments const *args)
{
    printf("CommandLinArguments(executable='%s', input='%s', algorithm='%s', "
           "output='%s')\n",
           args->executable,
           args->input_path,
           algorithm_names[args->algorithm],
           args->output_path);
}

static enum MRCAlgorithm
parse_algorithm_string(struct CommandLineArguments const *args, char *str)
{
    if (strcmp("Olken", str) == 0) {
        return MRC_ALGORITHM_OLKEN;
    } else if (strcmp("Fixed-Rate-SHARDS", str) == 0) {
        return MRC_ALGORITHM_FIXED_RATE_SHARDS;
    } else {
        LOGGER_ERROR("unparsable algorithm string: '%s'", str);
        print_help(stdout, args);
        exit(-1);
    }
    /* IMPOSSIBLE! */
    exit(-1);
}

static struct MissRateCurve
run_olken(struct Trace *trace)
{
    struct Olken me = {0};
    g_assert_true(Olken__init(&me, trace->length, 1));
    for (size_t i = 0; i < trace->length; ++i) {
        Olken__access_item(&me, trace->trace[i].key);
    }

    struct MissRateCurve mrc = {0};
    MissRateCurve__init_from_histogram(&mrc, &me.histogram);
    Olken__destroy(&me);
    return mrc;
}

static struct MissRateCurve
run_fixed_rate_shards(struct Trace *trace)
{
    struct FixedRateShards me = {0};
    g_assert_true(FixedRateShards__init(&me, trace->length, 1e-3, 1));
    for (size_t i = 0; i < trace->length; ++i) {
        FixedRateShards__access_item(&me, trace->trace[i].key);
    }

    struct MissRateCurve mrc = {0};
    MissRateCurve__init_from_histogram(&mrc, &me.olken.histogram);
    FixedRateShards__destroy(&me);
    return mrc;
}

int
main(int argc, char **argv)
{
    struct CommandLineArguments args = {0};
    args.executable = argv[0];

    // Set parameters based on user arguments
    for (int i = 1; i < argc; ++i) {
        if (matches_option(argv[i], "--input", "-i")) {
            ++i;
            if (i >= argc) {
                LOGGER_ERROR("expecting input path!");
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
    if (args.input_path == NULL) {
        LOGGER_ERROR("must specify input path!");
    }
    if (args.algorithm == MRC_ALGORITHM_INVALID) {
        LOGGER_ERROR("must specify algorithm!");
    }
    if (args.output_path == NULL) {
        LOGGER_ERROR("must specify output path!");
    }
    print_command_line_arguments(&args);

    // Read in trace
    struct Trace trace = read_trace(args.input_path);
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
        mrc = run_olken(&trace);
        break;
    case MRC_ALGORITHM_FIXED_RATE_SHARDS:
        mrc = run_fixed_rate_shards(&trace);
        break;
    default:
        LOGGER_ERROR("invalid algorithm %d", args.algorithm);
        exit(-1);
    }

    // Write out trace
    g_assert_true(MissRateCurve__write_binary_to_file(&mrc, args.output_path));
    MissRateCurve__destroy(&mrc);

    return 0;
}
