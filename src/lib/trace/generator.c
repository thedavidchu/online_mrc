#include <stdbool.h>
#include <stdlib.h>

#include "logger/logger.h"
#include "random/zipfian_random.h"
#include "trace/generator.h"
#include "trace/trace.h"

struct Trace
generate_zipfian_trace(const uint64_t length,
                       const uint64_t max_num_unique_entries,
                       const double skew,
                       const uint64_t seed)
{
    struct TraceItem *trace = NULL;
    struct ZipfianRandom zrng = {0};

    bool r = ZipfianRandom__init(&zrng, max_num_unique_entries, skew, seed);
    if (!r) {
        LOGGER_ERROR("couldn't initialize random number generator");
        goto cleanup;
    }

    trace = calloc(length, sizeof(*trace));
    if (trace == NULL) {
        LOGGER_ERROR("could not allocate return value");
        goto cleanup;
    }

    for (size_t i = 0; i < length; ++i) {
        trace[i] = (struct TraceItem){
            .key = ZipfianRandom__next(&zrng),
        };
    }

    ZipfianRandom__destroy(&zrng);
    return (struct Trace){.trace = trace, .length = length};

cleanup:
    free(trace);
    ZipfianRandom__destroy(&zrng);
    // Yes, I know I could just `return (struct Trace){0}`, but I want
    // to be VERY explicit.
    return (struct Trace){.trace = NULL, .length = 0};
}

struct Trace
generate_step_trace(const uint64_t length,
                    const uint64_t max_num_unique_entries)
{
    struct TraceItem *trace = NULL;

    trace = calloc(length, sizeof(*trace));
    if (trace == NULL) {
        LOGGER_ERROR("could not allocate return value");
        return (struct Trace){.trace = NULL, .length = 0};
    }

    for (size_t i = 0; i < length; ++i) {
        trace[i] = (struct TraceItem){
            .key = i % max_num_unique_entries,
        };
    }

    return (struct Trace){.trace = trace, .length = length};
}

struct Trace
generate_two_step_trace(const uint64_t length,
                        const uint64_t max_num_unique_entries)
{
    struct TraceItem *trace = NULL;

    trace = calloc(length, sizeof(*trace));
    if (trace == NULL) {
        LOGGER_ERROR("could not allocate return value");
        return (struct Trace){.trace = NULL, .length = 0};
    }

    if (max_num_unique_entries >= length / 2) {
        LOGGER_WARN(
            "length (%zu) must be greater than twice the desired number of "
            "unique entries (%zu)",
            length,
            max_num_unique_entries);
    }

    for (size_t i = 0; i < length / 2; ++i) {
        trace[i] = (struct TraceItem){
            .key = i % max_num_unique_entries,
        };
    }

    for (size_t i = length / 2; i < length; ++i) {
        trace[i] = (struct TraceItem){
            .key = i % (2 * max_num_unique_entries) + length / 2,
        };
    }

    return (struct Trace){.trace = trace, .length = length};
}
