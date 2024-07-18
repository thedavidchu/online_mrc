#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arrays/array_size.h"
#include "histogram/histogram.h"
#include "logger/logger.h"

enum MRCAlgorithm {
    MRC_ALGORITHM_INVALID,
    MRC_ALGORITHM_OLKEN,
    MRC_ALGORITHM_FIXED_RATE_SHARDS,
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
    "Fixed-Size-SHARDS",
    "QuickMRC",
    "Goel-QuickMRC",
    "Evicting-Map",
    "Average-Eviction-Time",
    "Their-Average-Eviction-Time",
};

static char const *const BOOLEAN_STRINGS[2] = {"false", "true"};

/// @brief  The arguments for running an instance.
/// @details
///     The standard algorithm contains the following information:
///     - Algorithm
///     - Output MRC path
///     - Output histogram path [optional]
///     - Sampling rate (if applicable) [optional. Default = by algorithm]
///     - Number of histogram bins [optional. Default = 1 << 20]
///     - Size of histogram bins [optional. Default = 1]
///     - Histogram overflow strategy [optional. Default = overflow]
///     - SHARDS adjustment [optional. Default = true for Fixed-Rate SHARDS]
///     The oracle contains:
///     - MRC path [both input/output]
///     - Histogram path [both input/output]
///     - Number of histogram bins [optional. Default = arbitrarily large]
///     - Size of histogram bins [optional. Default = 1]
///     - Histogram overflow strategy [optional. Default = overflow]
struct RunnerArguments {
    bool ok;

    enum MRCAlgorithm algorithm;
    char *mrc_path;
    char *hist_path;
    double sampling_rate;
    size_t num_bins;
    size_t bin_size;
    size_t max_size;
    enum HistogramOutOfBoundsMode out_of_bounds_mode;
    bool shards_adj;

    // This is inherited from the global arguments!
    bool cleanup;
};

char const *
maybe_string(char const *const str)
{
    return str == NULL ? "(null)" : str;
}

static inline char const *
bool_to_string(bool x)
{
    return x ? BOOLEAN_STRINGS[1] : BOOLEAN_STRINGS[0];
}

static enum MRCAlgorithm
parse_algorithm_string(char const *const str)
{
    for (size_t i = 1; i < ARRAY_SIZE(algorithm_names); ++i) {
        if (strcmp(algorithm_names[i], str) == 0)
            return (enum MRCAlgorithm)i;
    }
    LOGGER_ERROR("unparsable algorithm string: '%s'", str);
    return MRC_ALGORITHM_INVALID;
}

static inline bool
parse_bool(bool *value, char const *const str)
{
    for (size_t i = 0; i < ARRAY_SIZE(BOOLEAN_STRINGS); ++i) {
        if (strcmp(BOOLEAN_STRINGS[i], str) == 0) {
            *value = i;
            return true;
        }
    }
    return false;
}

static inline bool
parse_positive_size(size_t *size, char const *const str)
{
    char *endptr = NULL;
    unsigned long long u = strtoull(str, &endptr, 10);
    if (u == ULLONG_MAX && errno == ERANGE) {
        LOGGER_ERROR("integer (%s) out of range", str);
        return false;
    }
    if (&str[strlen(str)] != endptr) {
        LOGGER_ERROR("only the first %d characters of '%s' was interpreted",
                     endptr - str,
                     str);
        return false;
    }
    // I'm assuming the conversion for ULL to size_t is safe...
    *size = (size_t)u;
    return true;
}

static inline bool
parse_positive_double(double *value, char const *const str)
{
    char *endptr = NULL;
    double d = strtod(str, &endptr);
    if (d < 0.0 || (d == HUGE_VAL && errno == ERANGE)) {
        LOGGER_ERROR("number (%s) out of range", str);
        return false;
    }
    if (&str[strlen(str)] != endptr) {
        LOGGER_ERROR("only the first %d characters of '%s' was interpreted",
                     endptr - str,
                     str);
        return false;
    }
    *value = d;
    return true;
}

static void
print_help(void)
{
    fprintf(LOGGER_STREAM,
            ">>> Welcome to a tutorial on my very simple parser!\n");
    fprintf(LOGGER_STREAM,
            "    Format: "
            "<Algorithm>(mrc=<file>,hist=<file>,sampling=<float64-in-[0,1]>,"
            "num_bins=<positive-int>,bin_size=<positive-int>,"
            "max_size=<positive-int>,mode={allow_overflow,merge_bins,realloc},"
            "adj={true,false})\n");
    fprintf(LOGGER_STREAM,
            "    Example: "
            "Olken(mrc=olken-mrc.bin,hist=olken-hist.bin,sampling=1.0,num_bins="
            "100,bin_size=100,max_size=8000,mode=realloc,adj=false)\n");
    fprintf(
        LOGGER_STREAM,
        "    Notes: we reserve the use of the characters '(),='. There are no "
        "white spaces since these will not be stripped.\n");
}

static bool
parse_argument_string(struct RunnerArguments *const me, bool *no_more_args)
{
    // I just set this value so that we have some defined value by
    // default rather than an unknown value.
    *no_more_args = false;
    char *param = strtok(NULL, "=");
    // If this is the case, then we have no arguments!
    if (param == NULL || strcmp(param, ")") == 0) {
        *no_more_args = true;
        return true;
    }
    if (strcmp(param, "mrc") == 0) {
        char *value = strtok(NULL, ",)");
        if (value == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        me->mrc_path = strdup(value);
        return true;
    } else if (strcmp(param, "hist") == 0) {
        char *value = strtok(NULL, ",)");
        if (value == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        me->hist_path = strdup(value);
        return true;
    } else if (strcmp(param, "sampling") == 0) {
        char *value = strtok(NULL, ",)");
        if (value == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return parse_positive_double(&me->sampling_rate, value);
    } else if (strcmp(param, "num_bins") == 0) {
        char *value = strtok(NULL, ",)");
        if (value == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return parse_positive_size(&me->num_bins, value);
    } else if (strcmp(param, "bin_size") == 0) {
        char *value = strtok(NULL, ",)");
        if (value == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return parse_positive_size(&me->bin_size, value);
    } else if (strcmp(param, "max_size") == 0) {
        char *value = strtok(NULL, ",)");
        if (value == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return parse_positive_size(&me->max_size, value);
    } else if (strcmp(param, "mode") == 0) {
        char *value = strtok(NULL, ",)");
        if (value == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return HistogramOutOfBoundsMode__parse(&me->out_of_bounds_mode, value);
    } else if (strcmp(param, "adj") == 0) {
        char *value = strtok(NULL, ",)");
        if (value == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return parse_bool(&me->shards_adj, value);
    } else if (strcmp(param, "help") == 0) {
        print_help();
        return false;
    } else {
        LOGGER_ERROR("unrecognized parameter '%s'", param);
        print_help();
        return false;
    }
}

/// @brief  Parse an initialization string.
/// @details    My arbitrary format is thus:
///     "Algorithm(mrc=A,hist=B,sampling=C,num_bins=D,bin_size=E,mode=F,adj=G)"
/// @note   I do not allow spaces in case they are weirdly tokenized by
///         the shell.
/// @note   I do not follow the standard POSIX convention of arguments
///         that begin with a dash because, again, I do not want the
///         shell to parse these.
bool
RunnerArguments__init(struct RunnerArguments *const me,
                      char const *const str,
                      bool const cleanup)
{
    if (me == NULL || str == NULL) {
        return false;
    }
    // NOTE Not every algorithm uses all of these values. I have set
    //      each to a 'reasonable' value (except for 'sampling_rate')
    //      because I'd prefer not to debug crashes if the value is
    //      simply forgotten (e.g. if I set the 'max_size' to 'SIZE_MAX',
    //      then by not setting it, I get an error on allocating the
    //      hash table for the Evicting Map).
    *me = (struct RunnerArguments){.ok = false,
                                   .algorithm = MRC_ALGORITHM_INVALID,
                                   .mrc_path = NULL,
                                   .hist_path = NULL,
                                   .sampling_rate = 1.0,
                                   .num_bins = 1 << 20,
                                   .bin_size = 1,
                                   .max_size = 1 << 13,
                                   .out_of_bounds_mode =
                                       HistogramOutOfBoundsMode__allow_overflow,
                                   .shards_adj = true,
                                   .cleanup = cleanup};
    char *const garbage = strdup(str);
    if (garbage == NULL) {
        LOGGER_ERROR("bad strdup");
        return false;
    }
    char *tmp = garbage;

    char *algo_str = strtok(tmp, "(");
    if (algo_str == NULL) {
        LOGGER_ERROR("cannot parse algorithm from '%s'", str);
        goto cleanup;
    }
    me->algorithm = parse_algorithm_string(algo_str);
    if (me->algorithm == MRC_ALGORITHM_INVALID) {
        LOGGER_ERROR("invalid algorithm '%s'", algo_str);
        goto cleanup;
    }

    bool no_more_args = false;
    while (!no_more_args) {
        // NOTE This is totally C's fault but it stores state in the
        //      strtok function. Classic "seemed like a great idea at
        //      the time..." but can be very confusing. Or maybe I'm
        //      using it wrong. Either way, that's what's happening.
        if (!parse_argument_string(me, &no_more_args)) {
            LOGGER_ERROR("error in parsing argument string '%s'", str);
            return false;
        }
    }

    free(garbage);
    me->ok = true;
    return true;
cleanup:
    free(garbage);
    return false;
}

bool
RunnerArguments__println(struct RunnerArguments const *const me, FILE *const fp)
{
    if (me == NULL) {
        fprintf(fp, "RunnerArguments(null)\n");
        return false;
    }
    fprintf(fp,
            "RunnerArguments(algorithm=%s, mrc=%s, hist=%s, sampling=%g, "
            "num_bins=%zu, bin_size=%zu, max_size=%zu, mode=%s, adj=%s)\n",
            algorithm_names[me->algorithm],
            maybe_string(me->mrc_path),
            maybe_string(me->hist_path),
            me->sampling_rate,
            me->num_bins,
            me->bin_size,
            me->max_size,
            HISTOGRAM_MODE_STRINGS[me->out_of_bounds_mode],
            bool_to_string(me->shards_adj));
    return true;
}

void
RunnerArguments__destroy(struct RunnerArguments *const me)
{
    free(me->mrc_path);
    free(me->hist_path);
    *me = (struct RunnerArguments){0};
}
