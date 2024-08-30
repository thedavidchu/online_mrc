/** @brief  Test the fast and slow paths of Evicting-Map versus Fixed-Size
 *          SHARDS.
 *  @note   I rely on the fact that hash-based samplers ignore values
 *          whose hash is larger than some threshold.
 *  @note   I assume the hash is splitmix64 because that's the one that
 *          I can reverse!
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "arrays/reverse_index.h"
#include "hash/splitmix64.h"
#include "logger/logger.h"
#include "run/runner_arguments.h"
#include "run/trace_runner.h"
#include "trace/trace.h"

/// @brief  Generate a trace that will hammer a single item.
static struct Trace
generate_fastest_trace(size_t const trace_length)
{
    struct Trace trace = {0};
    if (!Trace__init(&trace, trace_length)) {
        LOGGER_ERROR("uninitialized trace");
        return (struct Trace){.trace = NULL, .length = 0};
    }
    for (size_t i = 0; i < trace_length; ++i) {
        trace.trace[i].key = 0;
    }
    return trace;
}

/// @brief  Generate a trace that will run quickly for hash-based samplers.
static struct Trace
generate_fast_trace(size_t const trace_length)
{
    struct Trace trace = {0};
    if (!Trace__init(&trace, trace_length)) {
        LOGGER_ERROR("uninitialized trace");
        return (struct Trace){.trace = NULL, .length = 0};
    }
    for (size_t i = 0; i < trace_length; ++i) {
        trace.trace[i].key = reverse_splitmix64_hash(i);
    }
    return trace;
}

/// @brief  Generate a trace that will run slowly for hash-based samplers.
static struct Trace
generate_slow_trace(size_t const trace_length)
{
    struct Trace trace = {0};
    if (!Trace__init(&trace, trace_length)) {
        LOGGER_ERROR("uninitialized trace");
        return (struct Trace){.trace = NULL, .length = 0};
    }
    for (size_t i = 0; i < trace_length; ++i) {
        trace.trace[i].key =
            reverse_splitmix64_hash(REVERSE_INDEX(i, trace_length));
    }
    return trace;
}

static bool
run_trace(char const *const *runner_args_array, struct Trace const *const trace)
{
    struct RunnerArguments args = {0};
    char const *s = NULL;
    while ((s = *runner_args_array++)) {
        if (!RunnerArguments__init(&args, s)) {
            LOGGER_ERROR("failed to initialize arguments");
            return false;
        }
        run_runner(&args, trace);
        RunnerArguments__destroy(&args);
    }
    return true;
}

int
main(void)
{
    size_t const trace_length = 1 << 20;
    struct Trace trace = {0};
    char const *runner_args_array[] = {"Evicting-Map(sampling=1e-1)",
                                       "Fixed-Size-SHARDS(sampling=1e-1)",
                                       NULL};
    // Test fastest trace
    trace = generate_fastest_trace(trace_length);
    if (!run_trace(runner_args_array, &trace)) {
        LOGGER_ERROR("fast path failed");
        exit(EXIT_FAILURE);
    }
    Trace__destroy(&trace);

    // Test fast trace
    trace = generate_fast_trace(trace_length);
    if (!run_trace(runner_args_array, &trace)) {
        LOGGER_ERROR("fast path failed");
        exit(EXIT_FAILURE);
    }
    Trace__destroy(&trace);

    // Test slow trace
    trace = generate_slow_trace(trace_length);
    if (!run_trace(runner_args_array, &trace)) {
        LOGGER_ERROR("slow path failed");
        exit(EXIT_FAILURE);
    }
    Trace__destroy(&trace);

    return 0;
}
