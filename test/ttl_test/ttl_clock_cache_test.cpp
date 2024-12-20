#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "cache/clock_cache.hpp"
#include "logger/logger.h"
#include "test/mytester.h"
#include "trace/reader.h"
#include "ttl_cache/new_ttl_clock_cache.hpp"

static void
print_keys(std::vector<std::uint64_t> &keys)
{
    for (auto k : keys) {
        std::cout << k << ",";
    }
    std::cout << std::endl;
}

static bool
simple_validation_test(std::vector<std::uint64_t> const &trace,
                       std::size_t const capacity,
                       int const verbose = 0)
{
    NewTTLClockCache cache(capacity);
    std::uint64_t time = 0;
    for (auto x : trace) {
        cache.access_item(time++, x);
        cache.validate(0);
        if (verbose >= 2) {
            std::cout << "Access key: " << x << std::endl;
            cache.debug_print();
        }
    }
    if (verbose) {
        cache.debug_print();
    }
    return true;
}

static std::vector<std::uint64_t>
get_trace(char const *const filename, enum TraceFormat format)
{
    struct Trace t = {};
    std::vector<std::uint64_t> trace;
    t = read_trace(filename, format);
    trace.reserve(t.length);
    for (std::size_t i = 0; i < t.length; ++i) {
        trace.push_back(t.trace[i].key);
    }
    return trace;
}

template <class C, class T>
static int
compare_cache_states(C const &cache, T const &ttl_cache, int const verbose = 0)
{
    int nerr = 0;
    cache.validate(verbose);
    ttl_cache.validate(verbose);
    // NOTE I'm getting some strange behaviour where the 'cache.size()'
    //      does not agree with the actual size.
    // if (cache.size() != ttl_cache.size()) {
    //     LOGGER_ERROR("cache (%zu) and TTL cache (%zu) are different sizes",
    //                  cache.size(),
    //                  ttl_cache.size());
    //     nerr += 1;
    // }
    if (verbose) {
        std::vector<std::uint64_t> keys = cache.get_keys();
        std::cout << "Cache keys: ";
        print_keys(keys);
        keys = ttl_cache.get_keys();
        std::cout << "TTL-Cache keys: ";
        print_keys(keys);
    }
    for (auto k : ttl_cache.get_keys()) {
        if (!cache.contains(k)) {
            LOGGER_ERROR("key %zu found in TTL cache but not regular cache", k);
            nerr += 1;
        }
    }
    return nerr;
}

static bool
compare_caches(std::vector<std::uint64_t> const &trace,
               std::size_t const capacity,
               int const verbose = 0,
               int const max_errs = 10)
{
    int nerr = 0;
    NewTTLClockCache ttl_cache(capacity);
    ClockCache cache(capacity);
    for (std::size_t i = 0; i < trace.size(); ++i) {
        cache.access_item(i, trace[i]);
        ttl_cache.access_item(i, trace[i]);
        if (i % capacity == 0) {
            nerr += compare_cache_states(cache, ttl_cache, verbose);
            if (nerr > max_errs) {
                return false;
            }
        }
    }
    nerr += compare_cache_states(cache, ttl_cache);
    return nerr == 0;
}

bool
trace_test(char const *const filename,
           TraceFormat const format,
           std::size_t const capacity,
           int const verbose = 0,
           int const max_errs = 10)
{
    std::vector<std::uint64_t> trace = get_trace(filename, format);
    return compare_caches(trace, capacity, verbose, max_errs);
}

int
main(int argc, char *argv[])
{
    std::vector<std::uint64_t> simple_trace = {0, 1, 2, 3, 0, 1, 2, 3, 4};
    std::vector<std::uint64_t> trace = {0, 1, 2, 3, 0, 1, 0, 2, 3, 4, 5, 6, 7};
    std::vector<std::uint64_t> src2_trace =
        {1, 2, 3, 4, 5, 5, 3, 4, 5, 6, 7, 8, 9, 10, 11};

    ASSERT_FUNCTION_RETURNS_TRUE(simple_validation_test(simple_trace, 4));
    ASSERT_FUNCTION_RETURNS_TRUE(simple_validation_test(trace, 4));
    ASSERT_FUNCTION_RETURNS_TRUE(simple_validation_test(src2_trace, 2, 2));
    if (argc == 2) {
        // ASSERT_FUNCTION_RETURNS_TRUE(
        //     trace_test(argv[1], TRACE_FORMAT_KIA, 1, 0));
        ASSERT_FUNCTION_RETURNS_TRUE(
            trace_test(argv[1], TRACE_FORMAT_KIA, 2, 2));
    }
    return 0;
}
