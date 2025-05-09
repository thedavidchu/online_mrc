#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arrays/array_size.h"
#include "arrays/is_last.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "lookup/dictionary.h"

#include "run/helper.h"
#include "run/runner_arguments.h"

// NOTE This corresponds to the same order as RunnerMode so that we can
//      simply use the enumeration to print the correct string!
static char *runner_mode_names[] = {
    "INVALID",
    "run",
    "tryread",
    "onlyread",
};

// NOTE This corresponds to the same order as MRCAlgorithm so that we can
//      simply use the enumeration to print the correct string!
char *algorithm_names[] = {
    "INVALID",
    "Oracle",
    "Olken",
    "Fixed-Rate-SHARDS",
    "Fixed-Size-SHARDS",
    "QuickMRC",
    "Goel-QuickMRC",
    "Evicting-Map",
    "Evicting-QuickMRC",
    "Average-Eviction-Time",
    "Their-Average-Eviction-Time",
};

static bool
parse_runner_mode_string(enum RunnerMode *value, char const *const str)
{
    if (value == NULL || str == NULL) {
        LOGGER_ERROR("cannot parse or return value");
        return false;
    }
    for (size_t i = 1; i < ARRAY_SIZE(runner_mode_names); ++i) {
        if (strcmp(runner_mode_names[i], str) == 0) {
            *value = (enum RunnerMode)i;
            return true;
        }
    }
    LOGGER_ERROR("unparsable runner mode string: '%s'", str);
    *value = RUNNER_MODE_INVALID;
    return false;
}

static void
print_algorithms_help_message(FILE *const stream)
{
    // NOTE I prefix the lines with '>' just so it's easier to read.
    fprintf(stream, "> Available algorithms are:\n");
    // NOTE algorithm_names[0] == "INVALID", so we skip this one.
    for (size_t i = 1; i < ARRAY_SIZE(algorithm_names); ++i) {
        fprintf(stream, "> \t- %s\n", algorithm_names[i]);
    }
    fprintf(stream,
            "> In oracle- or run-mode, 'Olken' uses the regular trace reader,\n"
            "> while 'Oracle' uses a page-by-page trace reader.\n"
            "> In TTL-mode, these are the same.\n");
    fflush(stream);
}

void
print_available_algorithms(FILE *stream)
{
    fprintf(stream, "{");
    // NOTE We want to skip the "INVALID" algorithm name (i.e. 0).
    for (size_t i = 1; i < ARRAY_SIZE(algorithm_names); ++i) {
        fprintf(stream, "%s", algorithm_names[i]);
        if (!is_last(i, ARRAY_SIZE(algorithm_names))) {
            fprintf(stream, ",");
        }
    }
    fprintf(stream, "}");
}

static enum MRCAlgorithm
parse_algorithm_string(char const *const str)
{
    for (size_t i = 1; i < ARRAY_SIZE(algorithm_names); ++i) {
        if (strcmp(algorithm_names[i], str) == 0)
            return (enum MRCAlgorithm)i;
    }
    LOGGER_ERROR("unparsable algorithm string: '%s'", str);
    print_algorithms_help_message(stdout);
    return MRC_ALGORITHM_INVALID;
}

static bool
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

static bool
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

static bool
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
            "<Algorithm>(runmode={run,tryread,onlyread},mrc=<file>,hist=<file>,"
            "sampling=<float64-in-[0,1]>,num_bins=<positive-int>,bin_size=<"
            "positive-int>,max_size=<positive-int>,mode={allow_overflow,merge_"
            "bins,realloc},adj={true,false},qmrc_size=<positive-int>)\n");
    fprintf(LOGGER_STREAM,
            "    Example: "
            "Olken(runmode=run,mrc=olken-mrc.bin,hist=olken-hist.bin,sampling="
            "1.0,num_bins=100,bin_size=100,max_size=8000,mode=realloc,adj="
            "false,qmrc_size=1)\n");
    fprintf(LOGGER_STREAM,
            "    Default: "
            "<INVALID>(runmode=run,mrc=(null),hist=(null),sampling=1.0,num_"
            "bins=1048576,bin_size=1,max_size=8192,mode=realloc,adj=true,qmrc_"
            "size=128)\n");
    fprintf(LOGGER_STREAM,
            "    Notes: we reserve the use of the characters '(),='. "
            "White spaces are not stripped.\n");
    fprintf(LOGGER_STREAM,
            "    Notes: any unrecognized (or misspelled) parameters will be "
            "stored in the generic dictionary, whose values are also subject "
            "to the same character constraints.");
    print_algorithms_help_message(LOGGER_STREAM);
}

static bool
parse_argument_string(struct RunnerArguments *const me, bool *no_more_args)
{
    assert(me != NULL);
    // I just set this value so that we have some defined value by
    // default rather than an unknown value.
    *no_more_args = false;
    char *param = strtok(NULL, "=");
    char *value = NULL;
    // If this is the case, then we have no arguments!
    if (param == NULL || strcmp(param, ")") == 0) {
        *no_more_args = true;
        return true;
    }
    if (strcmp(param, "runmode") == 0) {
        if ((value = strtok(NULL, ",)")) == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return parse_runner_mode_string(&me->run_mode, value);
    } else if (strcmp(param, "mrc") == 0) {
        if ((value = strtok(NULL, ",)")) == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        me->mrc_path = strdup(value);
        return true;
    } else if (strcmp(param, "hist") == 0) {
        if ((value = strtok(NULL, ",)")) == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        me->hist_path = strdup(value);
        return true;
    } else if (strcmp(param, "sampling") == 0) {
        if ((value = strtok(NULL, ",)")) == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return parse_positive_double(&me->sampling_rate, value);
    } else if (strcmp(param, "num_bins") == 0) {
        if ((value = strtok(NULL, ",)")) == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return parse_positive_size(&me->num_bins, value);
    } else if (strcmp(param, "bin_size") == 0) {
        if ((value = strtok(NULL, ",)")) == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return parse_positive_size(&me->bin_size, value);
    } else if (strcmp(param, "max_size") == 0) {
        if ((value = strtok(NULL, ",)")) == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return parse_positive_size(&me->max_size, value);
    } else if (strcmp(param, "mode") == 0) {
        if ((value = strtok(NULL, ",)")) == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return HistogramOutOfBoundsMode__parse(&me->out_of_bounds_mode, value);
    } else if (strcmp(param, "adj") == 0) {
        if ((value = strtok(NULL, ",)")) == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return parse_bool(&me->shards_adj, value);
    } else if (strcmp(param, "qmrc_size") == 0) {
        if ((value = strtok(NULL, ",)")) == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            return false;
        }
        return parse_positive_size(&me->qmrc_size, value);
    } else if (strcmp(param, "help") == 0) {
        print_help();
        return false;
    } else {
        if ((value = strtok(NULL, ",)")) == NULL) {
            LOGGER_ERROR("invalid value for parameter '%s'", param);
            print_help();
            return false;
        }
        LOGGER_WARN("unrecognized parameter '%s' with value '%s'. Storing it "
                    "in the dictionary!",
                    param,
                    value);
        return Dictionary__put(&me->dictionary, param, value);
    }
}

bool
RunnerArguments__init(struct RunnerArguments *const me, char const *const str)
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
    *me = (struct RunnerArguments){
        .ok = false,
        .run_mode = RUNNER_MODE_RUN,
        .algorithm = MRC_ALGORITHM_INVALID,
        .mrc_path = NULL,
        .hist_path = NULL,
        .sampling_rate = 1.0,
        .num_bins = 1 << 20,
        .bin_size = 1,
        .max_size = 1 << 13,
        .out_of_bounds_mode = HistogramOutOfBoundsMode__realloc,
        .shards_adj = true,
        // NOTE This should give us approximately 1% error.
        .qmrc_size = 128,
        .dictionary = (struct Dictionary){0},
    };

    if (!Dictionary__init(&me->dictionary)) {
        LOGGER_ERROR("failed to initialize dictionary");
        return false;
    }

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
    if (me->run_mode == RUNNER_MODE_INVALID) {
        LOGGER_ERROR("invalid runner mode '%s'", algo_str);
        goto cleanup;
    }
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
            "num_bins=%zu, bin_size=%zu, max_size=%zu, mode=%s, adj=%s, "
            "qmrc_size=%zu, dictionary=",
            algorithm_names[me->algorithm],
            maybe_string(me->mrc_path),
            maybe_string(me->hist_path),
            me->sampling_rate,
            me->num_bins,
            me->bin_size,
            me->max_size,
            HISTOGRAM_MODE_STRINGS[me->out_of_bounds_mode],
            bool_to_string(me->shards_adj),
            me->qmrc_size);
    Dictionary__write(&me->dictionary, fp, false);
    fprintf(fp, ")\n");
    return true;
}

void
RunnerArguments__destroy(struct RunnerArguments *const me)
{
    if (me == NULL) {
        return;
    }
    free(me->mrc_path);
    free(me->hist_path);
    Dictionary__destroy(&me->dictionary);
    *me = (struct RunnerArguments){0};
}
