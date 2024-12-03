/** @brief  Run the oracle in a memory-conserving fashion.
 *
 *  Methods to save memory include:
 *  - Not reading the entire trace into memory at once
 *
 * @note    This may drastically slow down our computation.
 */

#include <stdbool.h>

#include "file/file.h"
#include "histogram/histogram.h"
#include "io/io.h"
#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "olken/olken_with_ttl.h"
#include "run/runner_arguments.h"
#include "trace/reader.h"
#include "trace/trace.h"

/// @note   Aborting on an existing file is OK because we do this check
///         early. At least, that's how I intended it.
/// @param  mode:
///         1. run => no error if exists
///         2. tryread => warn if exists, but no error
///         3. onlyread => eror if exists
///
static bool
check_output_paths(enum RunnerMode const mode,
                   char const *const restrict hist_path,
                   char const *const restrict mrc_path)
{
    // Check for NULL strings
    if (mrc_path == NULL) {
        LOGGER_ERROR("MRC output path is NULL");
        return false;
    }
    if (hist_path == NULL) {
        LOGGER_ERROR("histogram output path is NULL");
        return false;
    }

    if (mode == RUNNER_MODE_RUN || mode == RUNNER_MODE_TRY_READ) {
        if (file_exists(mrc_path)) {
            LOGGER_WARN("MRC output path '%s' already exists", mrc_path);
        }
        if (file_exists(hist_path)) {
            LOGGER_WARN("histogram output path '%s' already exists", hist_path);
        }
        return true;
    } else if (mode == RUNNER_MODE_ONLY_READ) {
        bool ok = true;
        if (file_exists(mrc_path)) {
            LOGGER_WARN("MRC output path '%s' already exists", mrc_path);
            ok = false;
        }
        if (file_exists(hist_path)) {
            LOGGER_WARN("histogram output path '%s' already exists", hist_path);
            ok = false;
        }
        return ok;
    } else {
        LOGGER_ERROR("unrecognized runner mode %d", mode);
        return false;
    }
    return true;
}

bool
run_oracle(char const *const restrict trace_path,
           enum TraceFormat const format,
           struct RunnerArguments const *const args)
{
    LOGGER_TRACE("running 'run_oracle_with_ttl()");
    size_t const bytes_per_trace_item = get_bytes_per_trace_item(format);

    struct MemoryMap mm = {0};
    struct Olken olken = {0};
    struct MissRateCurve mrc = {0};
    size_t num_entries = 0;

    if (trace_path == NULL || args == NULL || bytes_per_trace_item == 0) {
        LOGGER_ERROR("invalid input", format);
        goto cleanup_error;
    }
    if (!check_output_paths(args->run_mode, args->hist_path, args->mrc_path)) {
        LOGGER_ERROR("error with output path, aborting!");
        goto cleanup_error;
    }

    // Memory map the input trace file
    if (!MemoryMap__init(&mm, trace_path, "rb")) {
        LOGGER_ERROR("failed to mmap '%s'", trace_path);
        goto cleanup_error;
    }
    num_entries = mm.num_bytes / bytes_per_trace_item;

    // Run trace
    if (!Olken__init_full(&olken,
                          args->num_bins,
                          args->bin_size,
                          HistogramOutOfBoundsMode__realloc)) {
        LOGGER_ERROR("failed to initialize Olken");
        goto cleanup_error;
    }
    for (size_t i = 0; i < num_entries; ++i) {
        if (i % 1000000 == 0) {
            LOGGER_TRACE("Finished %zu / %zu", i, num_entries);
        }
        struct TraceItemResult r = construct_trace_item(
            &((uint8_t *)mm.buffer)[i * bytes_per_trace_item],
            format);
        if (!r.valid) {
            continue;
        }
        Olken__access_item(&olken, r.item.key);
    }

    // Save histogram and MRC
    if (!MissRateCurve__init_from_histogram(&mrc, &olken.histogram)) {
        LOGGER_ERROR("failed to initialize MRC");
        goto cleanup_error;
    }
    if (!Histogram__save(&olken.histogram, args->hist_path)) {
        LOGGER_ERROR("failed to save histogram to '%s'", args->hist_path);
        goto cleanup_error;
    }
    if (!MissRateCurve__save(&mrc, args->mrc_path)) {
        LOGGER_ERROR("failed to save MRC to '%s'", args->mrc_path);
        goto cleanup_error;
    }

    MemoryMap__destroy(&mm);
    Olken__destroy(&olken);
    MissRateCurve__destroy(&mrc);
    return true;
cleanup_error:
    MemoryMap__destroy(&mm);
    Olken__destroy(&olken);
    MissRateCurve__destroy(&mrc);
    return false;
}

bool
run_oracle_with_ttl(char const *const restrict trace_path,
                    enum TraceFormat const format,
                    struct RunnerArguments const *const args)
{
    LOGGER_TRACE("running 'run_oracle_with_ttl()");
    size_t const bytes_per_trace_item = get_bytes_per_trace_item(format);

    struct MemoryMap mm = {0};
    struct OlkenWithTTL olken = {0};
    struct MissRateCurve mrc = {0};
    size_t num_entries = 0;

    if (trace_path == NULL || args == NULL || bytes_per_trace_item == 0) {
        LOGGER_ERROR("invalid input", format);
        goto cleanup_error;
    }
    if (!check_output_paths(args->run_mode, args->hist_path, args->mrc_path)) {
        LOGGER_ERROR("error with output path, aborting!");
        goto cleanup_error;
    }

    // Memory map the input trace file
    if (!MemoryMap__init(&mm, trace_path, "rb")) {
        LOGGER_ERROR("failed to mmap '%s'", trace_path);
        goto cleanup_error;
    }
    num_entries = mm.num_bytes / bytes_per_trace_item;

    // Run trace
    if (!OlkenWithTTL__init_full(&olken,
                                 args->num_bins,
                                 args->bin_size,
                                 HistogramOutOfBoundsMode__realloc,
                                 NULL)) {
        LOGGER_ERROR("failed to initialize Olken-with-TTL");
        goto cleanup_error;
    }
    for (size_t i = 0; i < num_entries; ++i) {
        if (i % 1000000 == 0) {
            LOGGER_TRACE("Finished %zu / %zu", i, num_entries);
        }
        struct FullTraceItemResult r = construct_full_trace_item(
            &((uint8_t *)mm.buffer)[i * bytes_per_trace_item],
            format);
        assert(r.valid);
        OlkenWithTTL__access_item(&olken,
                                  r.item.key,
                                  r.item.timestamp_ms,
                                  r.item.ttl_s);
    }

    // Save histogram and MRC
    if (!MissRateCurve__init_from_histogram(&mrc, &olken.olken.histogram)) {
        LOGGER_ERROR("failed to initialize MRC");
        goto cleanup_error;
    }
    if (!Histogram__save(&olken.olken.histogram, args->hist_path)) {
        LOGGER_ERROR("failed to save histogram to '%s'", args->hist_path);
        goto cleanup_error;
    }
    if (!MissRateCurve__save(&mrc, args->mrc_path)) {
        LOGGER_ERROR("failed to save MRC to '%s'", args->mrc_path);
        goto cleanup_error;
    }

    MemoryMap__destroy(&mm);
    OlkenWithTTL__destroy(&olken);
    MissRateCurve__destroy(&mrc);
    return true;
cleanup_error:
    MemoryMap__destroy(&mm);
    OlkenWithTTL__destroy(&olken);
    MissRateCurve__destroy(&mrc);
    return false;
}
