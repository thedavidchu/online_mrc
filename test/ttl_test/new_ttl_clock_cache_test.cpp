#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "cache/base_cache.hpp"
#include "cache/clock_cache.hpp"
#include "logger/logger.h"
#include "test/mytester.h"
#include "trace/reader.h"
#include "ttl_cache/base_ttl_cache.hpp"
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
simple_test()
{
    std::uint64_t trace[] = {0, 1, 2, 3, 0, 1, 2, 3, 4};
    NewTTLClockCache cache(4);

    std::uint64_t time = 0;
    for (auto x : trace) {
        cache.access_item(time++, x, 0);
        cache.validate(0);
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

static int
compare_caches(BaseCache const &cache,
               BaseTTLCache const &ttl_cache,
               int const verbose = 0)
{
    int nerr = 0;
    if (cache.size() != ttl_cache.size()) {
        LOGGER_ERROR("cache (%zu) and TTL cache (%zu) are different sizes",
                     cache.size(),
                     ttl_cache.size());
        nerr += 1;
    }
    if (verbose) {
        std::vector<std::uint64_t> keys = cache.get_keys();
        print_keys(keys);
        keys = ttl_cache.get_keys();
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
trace_test(char const *const filename,
           enum TraceFormat const format,
           std::size_t const capacity,
           int const verbose = 0)
{
    std::vector<std::uint64_t> trace = get_trace(filename, format);
    int nerr = 0;
    NewTTLClockCache cache(capacity);
    ClockCache oracle(capacity);
    for (std::size_t i = 0; i < trace.size(); ++i) {
        oracle.access_item(i, trace[i], 0);
        cache.access_item(i, trace[i], 0);
        if (i % capacity == 0) {
            nerr += compare_caches(oracle, cache, verbose);
            if (nerr > 10) {
                return false;
            }
        }
    }
    nerr += compare_caches(oracle, cache);
    if (nerr > 10) {
        return false;
    }
    return true;
}

int
main(int argc, char *argv[])
{
    ASSERT_FUNCTION_RETURNS_TRUE(simple_test());
    if (argc == 2) {
        ASSERT_FUNCTION_RETURNS_TRUE(
            trace_test(argv[1], TRACE_FORMAT_KIA, 1, 0));
        ASSERT_FUNCTION_RETURNS_TRUE(
            trace_test(argv[1], TRACE_FORMAT_KIA, 2, 1));
    }
    return 0;
}
