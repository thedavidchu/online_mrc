#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "arrays/array_size.h"
#include "cache/sieve_cache.hpp"
#include "logger/logger.h"
#include "test/mytester.h"
#include "trace/reader.h"
#include "ttl_cache/ttl_sieve_cache.hpp"
#include "yang_cache/yang_cache.hpp"
#include "yang_cache/yang_sieve_cache.hpp"

// NOTE This is the trace shown on the SIEVE website. Or at least, it is
//      one of the possible traces that causes the behaviour seen on the
//      SIEVE website.
//      Source: https://cachemon.github.io/SIEVE-website/
static std::uint64_t short_trace[] = {
    'A',
    'A',
    'B',
    'B',
    'C',
    'D',
    'E',
    'F',
    'G',
    'G',
    'H',
    'A',
    'D',
    'I',
    'B',
    'J',
};
static std::string soln[] = {
    "||",
    "|A|",
    "|A|",
    "|B|A|",
    "|B|A|",
    "|C|B|A|",
    "|D|C|B|A|",
    "|E|D|C|B|A|",
    "|F|E|D|C|B|A|",
    "|G|F|E|D|C|B|A|",
    // This is where the example starts
    "|G|F|E|D|C|B|A|",
    "|H|G|F|E|D|B|A|",
    "|H|G|F|E|D|B|A|",
    "|H|G|F|E|D|B|A|",
    "|I|H|G|F|D|B|A|",
    "|I|H|G|F|D|B|A|",
    "|J|I|H|G|D|B|A|",
};

static std::string
sieve_print(std::vector<std::uint64_t> keys)
{
    std::string s;
    for (auto key : keys) {
        // NOTE We know that the key is a character.
        s = std::string(1, (char)key) + "|" + s;
    }
    if (s == "") {
        return "||";
    }
    return "|" + s;
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

static bool
my_simple_test()
{
    std::vector<std::string> my_soln;
    SieveCache cache(7);

    std::string s = sieve_print(cache.get_keys_in_eviction_order());
    my_soln.push_back(s);
    for (std::size_t i = 0; i < ARRAY_SIZE(short_trace); ++i) {
        cache.access_item({i, short_trace[i]});
        s = sieve_print(cache.get_keys_in_eviction_order());
        my_soln.push_back(s);
    }

    // Print my solution
    for (auto s : my_soln) {
        std::cout << s << std::endl;
    }

    assert(my_soln.size() == ARRAY_SIZE(soln));
    for (std::size_t i = 0; i < my_soln.size(); ++i) {
        if (my_soln[i] != soln[i]) {
            LOGGER_ERROR("mismatching strings at %zu: got '%s', expecting '%s'",
                         i,
                         my_soln[i].c_str(),
                         soln[i].c_str());
        }
    }

    return true;
}

/// @brief  Check the external implementation matches the example given
///         by Yang et al. on their blog.
static bool
yang_simple_test()
{
    // Check external solution
    std::vector<std::string> ext_alt_soln;
    YangCache ext_cache_alt(7, YangCacheType::SIEVE);
    std::string s;
    s = sieve_print(ext_cache_alt.get_keys());
    ext_alt_soln.push_back(s);
    for (std::size_t i = 0; i < ARRAY_SIZE(short_trace); ++i) {
        ext_cache_alt.access_item({i, short_trace[i]});
        s = sieve_print(ext_cache_alt.get_keys());
        ext_alt_soln.push_back(s);
    }
    assert(ext_alt_soln.size() == ARRAY_SIZE(soln));
    for (std::size_t i = 0; i < ext_alt_soln.size(); ++i) {
        if (ext_alt_soln[i] != soln[i]) {
            LOGGER_ERROR(
                "(alt) mismatching strings at %zu: got '%s', expecting '%s'",
                i,
                ext_alt_soln[i].c_str(),
                soln[i].c_str());
        }
    }
    return true;
}

template <typename T>
static void
print_vector(std::vector<T> vec)
{
    std::cout << "{";
    for (T x : vec) {
        std::cout << x << ",";
    }
    std::cout << "}" << std::endl;
}

/// @note   Compare the caches
static int
compare_caches(TTLSieveCache const &my_cache, YangCache const &yang_cache)
{
    int nerr = 0;
    int const MAX_NERRS = 10;

    // NOTE If the caches are the same size, then we know that all of
    //      'missing' keys in one cache have corresponding 'missing'
    //      other keys in the other cache.
    if (my_cache.size() != yang_cache.size()) {
        LOGGER_ERROR("different size caches %zu vs %zu",
                     my_cache.size(),
                     yang_cache.size());
        ++nerr;
    }
    for (auto k : my_cache.get_keys_in_eviction_order()) {
        if (!yang_cache.contains(k)) {
            LOGGER_ERROR("Yang's cache missing key '%zu'", k);
            print_vector(my_cache.get_keys_in_eviction_order());
            print_vector(yang_cache.get_keys());
            ++nerr;
            if (nerr > MAX_NERRS) {
                return nerr;
            }
        }
    }
    return nerr;
}

/// @brief  Compare the state of two SIEVE cache implementations.
static bool
comparison_sieve_test(std::size_t capacity, std::vector<std::uint64_t> trace)
{
    // Cap the number of errors that we report to the user.
    int nerr = 0;
    int const MAX_NERRS = 10;
    LOGGER_INFO("Testing SIEVE cache with capacity %zu", capacity);
    TTLSieveCache my_cache(capacity);
    YangCache yang_cache(capacity, YangCacheType::SIEVE);
    for (std::size_t i = 0; i < trace.size(); ++i) {
        auto key = trace[i];
        my_cache.access_item({i, key});
        yang_cache.access_item({i, key});

        // NOTE This should amoritize the cost of comparisons to O(N).
        if (i % capacity == 0) {
            int n = compare_caches(my_cache, yang_cache);
            if (n) {
                LOGGER_ERROR("mismatch on iteration %zu", i);
            }
            nerr += n;
        }
        // This is so that we print a few errors at once, but also do
        // not overwhelm the system with billions of errors if things
        // get really bad!
        if (nerr > MAX_NERRS) {
            return false;
        }
    }
    return nerr == 0;
}

int
main(int argc, char *argv[])
{
    ASSERT_FUNCTION_RETURNS_TRUE(my_simple_test());
    ASSERT_FUNCTION_RETURNS_TRUE(yang_simple_test());

    if (argc == 1) {
        LOGGER_WARN("skipping real trace test");
        return 0;
    }
    // NOTE I assume the trace we're being passed is MSR src2.bin.
    std::vector<std::uint64_t> trace = get_trace(argv[1], TRACE_FORMAT_KIA);
    ASSERT_FUNCTION_RETURNS_TRUE(comparison_sieve_test(2, trace));
    ASSERT_FUNCTION_RETURNS_TRUE(comparison_sieve_test(1 << 10, trace));
    ASSERT_FUNCTION_RETURNS_TRUE(comparison_sieve_test(1 << 11, trace));
    ASSERT_FUNCTION_RETURNS_TRUE(comparison_sieve_test(1 << 12, trace));
    ASSERT_FUNCTION_RETURNS_TRUE(comparison_sieve_test(1 << 13, trace));
    ASSERT_FUNCTION_RETURNS_TRUE(comparison_sieve_test(1 << 14, trace));
    ASSERT_FUNCTION_RETURNS_TRUE(comparison_sieve_test(1 << 15, trace));
    return 0;
}
