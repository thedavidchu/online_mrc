#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "evicting_map/evicting_map.h"
#include "evicting_quickmrc/evicting_quickmrc.h"
#include "file/file.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "shards/fixed_rate_shards.h"
#include "shards/fixed_size_shards.h"
#include "timer/timer.h"
#include "trace/generator.h"
#include "trace/trace.h"

#include "run/runner_arguments.h"

/// @note   The keyword 'inline' prevents a compiler warning as per:
///         https://stackoverflow.com/questions/32432596/warning-always-inline-function-might-not-be-inlinable-wattributes
#define forceinline __attribute__((always_inline)) inline

/// @note   I forcibly inline this with the hope that the compiler will
///         be able to realize that the function pointers are constants.
///         I noticed an improvement from 8.2s to 7.6s on the Twitter
///         trace, cluster15.bin.
static forceinline bool
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

    if (runner_data == NULL || args == NULL || trace == NULL ||
        access_func == NULL || postprocess_func == NULL || hist_func == NULL ||
        destroy_func == NULL) {
        LOGGER_ERROR("arguments cannot be NULL!");
        return false;
    }

    if (args->run_mode == RUNNER_MODE_TRY_READ) {
        if (file_exists(args->mrc_path) && file_exists(args->hist_path)) {
            LOGGER_INFO("skipping %s to read existing files",
                        algorithm_names[args->algorithm]);
            goto ok_cleanup;
        } else {
            LOGGER_INFO(
                "MRC and/or histogram files don't exist, so running normally");
        }
    }

    double const t0 = get_wall_time_sec();
    for (size_t i = 0; i < trace->length; ++i) {
        // NOTE I really, really, really hope that the compiler is smart
        //      enough to inline this function!!!
        access_func(runner_data, trace->trace[i].key);
        if (i % 1000000 == 0) {
            LOGGER_TRACE("Finished %zu / %zu", i, trace->length);
        }
    }
    double const t1 = get_wall_time_sec();
    // NOTE In the future, we will not require users to create a post-
    //      process function, but rather let it be NULL.
    if (postprocess_func != NULL) {
        postprocess_func(runner_data);
    }
    double const t2 = get_wall_time_sec();
    // NOTE We do NOT own the histogram data through the 'hist' object.
    //      The 'runner_data' object maintains ownership of the data.
    if (!hist_func(runner_data, &hist)) {
        LOGGER_ERROR("histogram getter failed");
        goto error_cleanup;
    }
    if (!MissRateCurve__init_from_histogram(&mrc, hist)) {
        LOGGER_ERROR("MRC initialization failed");
        goto error_cleanup;
    }
    double const t3 = get_wall_time_sec();
    LOGGER_INFO("%s -- Histogram Time: %f | Post-Process Time: %f | MRC Time: "
                "%f | Total Time: %f",
                algorithm_names[args->algorithm],
                t1 - t0,
                t2 - t1,
                t3 - t2,
                t3 - t0);
    if (args->hist_path != NULL) {
        bool save_hist = true;
        if (file_exists(args->hist_path)) {
            LOGGER_WARN("file '%s' already exists!", args->hist_path);
        }
        if (save_hist && !Histogram__save(hist, args->hist_path)) {
            LOGGER_WARN("failed to save histogram in '%s'", args->hist_path);
        }
    }
    if (args->mrc_path != NULL) {
        bool save_mrc = true;
        if (file_exists(args->mrc_path)) {
            LOGGER_WARN("file '%s' already exists!", args->mrc_path);
        }
        if (save_mrc && !MissRateCurve__save(&mrc, args->mrc_path)) {
            LOGGER_WARN("failed to save MRC in '%s'", args->mrc_path);
        }
    }
ok_cleanup:
    destroy_func(runner_data);
    MissRateCurve__destroy(&mrc);
    return true;
error_cleanup:
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

static bool
run_evicting_quickmrc(struct RunnerArguments const *const args,
                      struct Trace const *const trace)
{
    struct EvictingQuickMRC me = {0};
    if (!EvictingQuickMRC__init(&me,
                                args->sampling_rate,
                                args->max_size,
                                args->qmrc_size,
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
        (bool (*)(void *const, uint64_t const))EvictingQuickMRC__access_item,
        (bool (*)(void *const))EvictingQuickMRC__post_process,
        (bool (*)(void *const, struct Histogram const **const))
            EvictingQuickMRC__get_histogram,
        (void (*)(void *const))EvictingQuickMRC__destroy);
}

bool
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
    case MRC_ALGORITHM_EVICTING_QUICKMRC:
        if (!run_evicting_quickmrc(args, trace)) {
            LOGGER_WARN("Evicting QuickMRC failed");
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
