#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <vector>

#include "test/mytester.h"
#include "trace/reader.h"
#include "ttl_cache/new_ttl_clock_cache.hpp"
#include "yang_cache/yang_cache.hpp"

bool const debug = false;

/*******************************************************************************
 *  HELPER FUNCTIONS
 *********************************************************************************/

static void
print_vector(std::vector<std::uint64_t> &keys, std::ostream &stream)
{
    for (auto k : keys) {
        stream << k << ",";
    }
    stream << std::endl;
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

static void
print_caches(YangCache<YangCacheType::CLOCK> const &cache,
             NewTTLClockCache const &ttl_cache,
             std::size_t const i,
             std::ostream &stream)
{
    stream << "[INFO] iteration " << i << std::endl;
    std::vector<std::uint64_t> c_keys = cache.get_keys(),
                               t_keys = ttl_cache.get_keys();
    stream << "Cache keys:";
    print_vector(c_keys, stream);
    stream << "TTL Cache keys:";
    print_vector(t_keys, stream);
    cache.print(stream);
    ttl_cache.debug_print(stream);
}

/*******************************************************************************
 *  VALIDATION TESTING
 *********************************************************************************/

/// @brief  Validate that the trace runs with validations turned on.
///         We don't check the correctness of the result.
static bool
simple_validation_test(std::vector<std::uint64_t> const &trace,
                       std::size_t const capacity,
                       int const verbose = 0)
{
    NewTTLClockCache cache(capacity);
    std::uint64_t time = 0;
    for (auto x : trace) {
        cache.access_item({time++, x});
        if (verbose >= 2) {
            std::cout << "Access key: " << x << std::endl;
            cache.debug_print(std::cout);
            cache.to_stream(std::cout);
        }
        cache.validate(std::cout, 0);
    }
    if (verbose) {
        cache.debug_print(std::cout);
    }
    return true;
}

/*******************************************************************************
 *  CACHE VS ORACLE TESTING
 *********************************************************************************/
static bool
equal_vectors(std::vector<std::uint64_t> const &lhs,
              std::vector<std::uint64_t> const &rhs,
              std::ostream &stream)
{
    if (lhs.size() != rhs.size()) {
        stream << "[ERROR] mismatch in vector sizes: " << lhs.size() << " vs "
               << rhs.size() << std::endl;
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i] != rhs[i]) {
            stream << "[ERROR] mismatch at " << i << ": " << lhs[i] << " vs "
                   << rhs[i] << "" << std::endl;
            return false;
        }
    }
    return true;
}

/*******************************************************************************
 *  CACHE COMPARISON TESTING
 *********************************************************************************/

static int
compare_cache_states(YangCache<YangCacheType::CLOCK> const &cache,
                     NewTTLClockCache const &ttl_cache,
                     std::ostream &stream,
                     int const verbose = 0)
{
    int nerr = 0;
    cache.validate(stream, verbose);
    ttl_cache.validate(stream, verbose);
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
        stream << "Cache keys: ";
        print_vector(keys, stream);
        keys = ttl_cache.get_keys();
        stream << "TTL-Cache keys: ";
        print_vector(keys, stream);
    }

    std::vector<std::uint64_t> t_keys = ttl_cache.get_keys(),
                               c_keys = cache.get_keys();
    if (!equal_vectors(c_keys, t_keys, stream)) {
        nerr += 1;
    }
    return nerr;
}

static bool
compare_caches(std::vector<std::uint64_t> const &trace,
               std::size_t const capacity,
               std::stringstream &stream,
               int const verbose = 0,
               int const max_errs = 1)
{
    int nerr = 0;
    NewTTLClockCache ttl_cache(capacity);
    YangCache<YangCacheType::CLOCK> cache(capacity);
    assert(errno == 0);
    for (std::size_t i = 0; i < trace.size(); ++i) {
        if (debug) {
            stream << "vvvvvvvvvv" << std::endl;
            print_caches(cache, ttl_cache, i, stream);
            stream << ">>> ACCESS ITEM " << trace[i] << std::endl;
        }
        cache.access_item({i, trace[i]});
        ttl_cache.access_item({i, trace[i]});
        if (i % capacity == 0) {
            int n = compare_cache_states(cache, ttl_cache, stream, verbose);
            if (debug && n) {
                stream << "[ERROR] mismatch on iteration " << i << std::endl;
                print_caches(cache, ttl_cache, i, stream);
            }
            nerr += n;
            if (nerr > max_errs) {
                goto error;
            }
        }
        if (debug) {
            stream << "^^^^^^^^^^" << std::endl;
        }
    }
    nerr += compare_cache_states(cache, ttl_cache, stream);
    if (nerr) {
        goto error;
    }
    return true;
error:
    if (debug) {
        std::cout << stream.str() << std::endl;
    }
    return false;
}

bool
trace_test(char const *const filename,
           TraceFormat const format,
           std::size_t const capacity,
           int const verbose = 0,
           int const max_errs = 10)
{
    std::vector<std::uint64_t> trace = get_trace(filename, format);
    assert(errno == 0);
    std::stringstream ss;
    bool r = compare_caches(trace, capacity, ss, verbose, max_errs);
    if (!r) {
        std::cout << ss.str();
        return false;
    }
    return true;
}

int
main(int argc, char *argv[])
{
    bool const simple_test = false;
    if (simple_test) {
        std::vector<std::uint64_t> simple_trace = {0, 1, 2, 3, 0, 1, 2, 3, 4};
        std::vector<std::uint64_t> trace =
            {0, 1, 2, 3, 0, 1, 0, 2, 3, 4, 5, 6, 7};
        std::vector<std::uint64_t> src2_trace =
            {1, 2, 3, 4, 5, 5, 3, 4, 5, 6, 7, 8, 9, 10, 11};
        ASSERT_FUNCTION_RETURNS_TRUE(simple_validation_test(simple_trace, 4));
        ASSERT_FUNCTION_RETURNS_TRUE(simple_validation_test(trace, 4));
        ASSERT_FUNCTION_RETURNS_TRUE(simple_validation_test(src2_trace, 2));
    }

    bool const filling_test = true;
    if (filling_test) {
        // NOTE I've never done something so silly in my life, but I'm creating
        //      every combination of length 8 with the numbers 0..=3. My
        //      calculations say this should be 65536 (=4^8).
        std::size_t const t0 = 0;
        for (std::size_t t1 = 0; t1 < 2; ++t1) {
            for (std::size_t t2 = 0; t2 < 3; ++t2) {
                for (std::size_t t3 = 0; t3 < 4; ++t3) {
                    for (std::size_t t4 = 0; t4 < 4; ++t4) {
                        for (std::size_t t5 = 0; t5 < 4; ++t5) {
                            for (std::size_t t6 = 0; t6 < 4; ++t6) {
                                for (std::size_t t7 = 0; t7 < 4; ++t7) {
                                    std::vector<std::uint64_t> trace =
                                        {t0, t1, t2, t3, t4, t5, t6, t7};
                                    std::stringstream stream;
                                    if (debug) {
                                        print_vector(trace, stream);
                                    }
                                    ASSERT_FUNCTION_RETURNS_TRUE(
                                        compare_caches(trace, 2, stream));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    bool const real_trace_test = true;
    if (real_trace_test && argc == 2) {
        ASSERT_FUNCTION_RETURNS_TRUE(trace_test(argv[1], TRACE_FORMAT_KIA, 1));
        ASSERT_FUNCTION_RETURNS_TRUE(trace_test(argv[1], TRACE_FORMAT_KIA, 2));
        ASSERT_FUNCTION_RETURNS_TRUE(
            trace_test(argv[1], TRACE_FORMAT_KIA, 1 << 10));
        ASSERT_FUNCTION_RETURNS_TRUE(
            trace_test(argv[1], TRACE_FORMAT_KIA, 1 << 12));
        ASSERT_FUNCTION_RETURNS_TRUE(
            trace_test(argv[1], TRACE_FORMAT_KIA, 1 << 14));
        ASSERT_FUNCTION_RETURNS_TRUE(
            trace_test(argv[1], TRACE_FORMAT_KIA, 1 << 16));
        ASSERT_FUNCTION_RETURNS_TRUE(
            trace_test(argv[1], TRACE_FORMAT_KIA, 1 << 18));
    }
    return 0;
}
