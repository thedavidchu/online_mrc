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
#include "unused/mark_unused.h"

/// @note   The keyword 'inline' prevents a compiler warning as per:
///         https://stackoverflow.com/questions/32432596/warning-always-inline-function-might-not-be-inlinable-wattributes
#define forceinline __attribute__((always_inline)) inline

static char const *const SHORT_RUN[] = {
    "Evicting-Map(sampling=1e-1,max_size=8192)",
    "Fixed-Size-SHARDS(sampling=1e-1,max_size=8192)",
    NULL};
static char const *const LONG_RUN[] = {
    "Evicting-Map(sampling=1e-1,max_size=65536)",
    "Evicting-Map(sampling=1e-1,max_size=32768)",
    "Evicting-Map(sampling=1e-1,max_size=16384)",
    "Evicting-Map(sampling=1e-1,max_size=8192)",
    "Evicting-Map(sampling=1e-1,max_size=4096)",
    "Evicting-Map(sampling=1e-1,max_size=2048)",
    "Evicting-Map(sampling=1e-1,max_size=1024)",
    "Evicting-Map(sampling=1e-1,max_size=512)",
    "Evicting-Map(sampling=1e-1,max_size=256)",
    "Evicting-Map(sampling=1e-1,max_size=128)",
    "Evicting-Map(sampling=1e-1,max_size=64)",
    "Evicting-Map(sampling=1e-1,max_size=32)",
    "Fixed-Size-SHARDS(sampling=1e-1,max_size=65536)",
    "Fixed-Size-SHARDS(sampling=1e-1,max_size=32768)",
    "Fixed-Size-SHARDS(sampling=1e-1,max_size=16384)",
    "Fixed-Size-SHARDS(sampling=1e-1,max_size=8192)",
    "Fixed-Size-SHARDS(sampling=1e-1,max_size=4096)",
    "Fixed-Size-SHARDS(sampling=1e-1,max_size=2048)",
    "Fixed-Size-SHARDS(sampling=1e-1,max_size=1024)",
    "Fixed-Size-SHARDS(sampling=1e-1,max_size=512)",
    "Fixed-Size-SHARDS(sampling=1e-1,max_size=256)",
    "Fixed-Size-SHARDS(sampling=1e-1,max_size=128)",
    "Fixed-Size-SHARDS(sampling=1e-1,max_size=64)",
    "Fixed-Size-SHARDS(sampling=1e-1,max_size=32)",
    NULL};

/// @brief  Generate a trace from a function.
static forceinline struct Trace
generate_trace(size_t const trace_length,
               uint64_t (*const f)(size_t const i, size_t const trace_length))
{
    struct Trace trace = {0};
    if (!Trace__init(&trace, trace_length)) {
        LOGGER_ERROR("uninitialized trace");
        return (struct Trace){.trace = NULL, .length = 0};
    }
    for (size_t i = 0; i < trace_length; ++i) {
        trace.trace[i].key = f(i, trace_length);
    }
    return trace;
}

static uint64_t
hammer_single_element(size_t const i, size_t const trace_length)
{
    UNUSED(i);
    UNUSED(trace_length);
    return 0;
}

/// @note   This will cause the sampling methods to run quickly because
///         we are increasing the hashes, so later entries won't get
///         sampled.
static uint64_t
increasing_hashes(size_t const i, size_t const trace_length)
{
    UNUSED(trace_length);
    return reverse_splitmix64_hash(i);
}

static uint64_t
random_hashes(size_t const i, size_t const trace_length)
{
    UNUSED(trace_length);
    return i;
}

static uint64_t
decreasing_hashes(size_t const i, size_t const trace_length)
{
    return reverse_splitmix64_hash(REVERSE_INDEX(i, trace_length));
}

/// @note   This function is used to generate a series of numbers that
///         cause a decreasing hash but are not a simple constant away
///         from each other, in order to defeat the hardware prefetcher.
static uint64_t
decreasing_nonstrided_hashes(size_t const i, size_t const trace_length)
{
    return reverse_splitmix64_hash(
        REVERSE_INDEX(i * i * i, trace_length * trace_length * trace_length));
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
    MAYBE_UNUSED(SHORT_RUN);
    MAYBE_UNUSED(LONG_RUN);
    char const *const *const runner_args_array = SHORT_RUN;
    bool const run_hammer = true, run_fast = true, run_random = true,
               run_slow = true, run_slowest = true;
    // Test fastest trace
    trace = generate_trace(trace_length, hammer_single_element);
    if (run_hammer && !run_trace(runner_args_array, &trace)) {
        LOGGER_ERROR("single-element hammer failed");
        exit(EXIT_FAILURE);
    }
    Trace__destroy(&trace);

    // Test fast trace
    trace = generate_trace(trace_length, increasing_hashes);
    if (run_fast && !run_trace(runner_args_array, &trace)) {
        LOGGER_ERROR("fast path failed");
        exit(EXIT_FAILURE);
    }
    Trace__destroy(&trace);

    // Test random trace
    trace = generate_trace(trace_length, random_hashes);
    if (run_random && !run_trace(runner_args_array, &trace)) {
        LOGGER_ERROR("random path failed");
        exit(EXIT_FAILURE);
    }
    Trace__destroy(&trace);

    // Test slow trace
    trace = generate_trace(trace_length, decreasing_hashes);
    if (run_slow && !run_trace(runner_args_array, &trace)) {
        LOGGER_ERROR("slow path failed");
        exit(EXIT_FAILURE);
    }
    Trace__destroy(&trace);

    // Test slowest trace
    trace = generate_trace(trace_length, decreasing_nonstrided_hashes);
    if (run_slowest && !run_trace(runner_args_array, &trace)) {
        LOGGER_ERROR("slowest path failed");
        exit(EXIT_FAILURE);
    }
    Trace__destroy(&trace);

    return 0;
}
