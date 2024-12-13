#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "arrays/array_size.h"
#include "cache/sieve_cache.hpp"
#include "external_sieve.hpp"
#include "logger/logger.h"
#include "test/mytester.h"
#include "trace/reader.h"
#include "ttl_cache/ttl_sieve_cache.hpp"

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
simple_test()
{
    std::vector<std::string> my_soln;
    SieveCache cache(7);

    std::string s = sieve_print(cache.get_keys_in_eviction_order());
    my_soln.push_back(s);
    for (auto i : short_trace) {
        cache.access_item(i);
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
simple_external_test()
{
    // Check external solution
    std::vector<std::string> ext_soln;
    ExternalSieveCache<std::uint64_t, std::uint64_t> ext_cache(7);
    std::string s = sieve_print(ext_cache.get_keys_in_eviction_order());
    ext_soln.push_back(s);
    for (auto i : short_trace) {
        ext_cache.insert(i, 0);
        s = sieve_print(ext_cache.get_keys_in_eviction_order());
        ext_soln.push_back(s);
    }
    assert(ext_soln.size() == ARRAY_SIZE(soln));
    for (std::size_t i = 0; i < ext_soln.size(); ++i) {
        if (ext_soln[i] != soln[i]) {
            LOGGER_ERROR("mismatching strings at %zu: got '%s', expecting '%s'",
                         i,
                         ext_soln[i].c_str(),
                         soln[i].c_str());
        }
    }
    return true;
}

static bool
short_external_sieve_test()
{
    bool ok = true;
    SieveCache my_cache(7);
    ExternalSieveCache<std::uint64_t, std::uint64_t> ext_cache(7);

    for (auto key : short_trace) {
        my_cache.access_item(key);
        ext_cache.insert(key, 0);

        if (my_cache.size() != ext_cache.length()) {
            LOGGER_ERROR("different size caches %zu vs %zu",
                         my_cache.size(),
                         ext_cache.length());
            ok = false;
        }
        for (auto k : my_cache.get_keys_in_eviction_order()) {
            if (!ext_cache.contains(k)) {
                LOGGER_ERROR("external cache missing key '%zu'", k);
                ok = false;
            }
        }
    }
    return ok;
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

static bool
external_sieve_test(std::size_t capacity, std::vector<std::uint64_t> trace)
{
    bool ok = true;
    // Cap the number of errors that we report to the user.
    int nerr = 0;
    LOGGER_INFO("Testing SIEVE cache with capacity %zu", capacity);
    TTLSieveCache my_cache(capacity);
    ExternalSieveCache<std::uint64_t, std::uint64_t> ext_cache(capacity);
    for (std::size_t i = 0; i < trace.size(); ++i) {
        auto key = trace[i];
        my_cache.access_item(key);
        ext_cache.insert(key, 0);
        if (my_cache.size() != ext_cache.length()) {
            LOGGER_ERROR("iteration %zu: different size caches %zu vs %zu",
                         i,
                         my_cache.size(),
                         ext_cache.length());
            ok = false;
            ++nerr;
            if (nerr > 10) {
                return false;
            }
        }
        for (auto k : my_cache.get_keys_in_eviction_order()) {
            if (!ext_cache.contains(k)) {
                LOGGER_ERROR("iteration %zu, external cache missing key '%zu'",
                             i,
                             k);
                print_vector(my_cache.get_keys_in_eviction_order());
                print_vector(ext_cache.get_keys_in_eviction_order());
                ok = false;
                ++nerr;
                if (nerr > 10) {
                    return false;
                }
            }
        }
    }
    return ok;
}

int
main(int argc, char *argv[])
{
    ASSERT_FUNCTION_RETURNS_TRUE(simple_test());
    ASSERT_FUNCTION_RETURNS_TRUE(simple_external_test());
    ASSERT_FUNCTION_RETURNS_TRUE(short_external_sieve_test());

    if (argc == 1) {
        LOGGER_WARN("skipping real trace test");
        return 0;
    }
    // NOTE I assume the trace we're being passed is MSR src2.bin.
    std::vector<std::uint64_t> trace = get_trace(argv[1], TRACE_FORMAT_KIA);
    ASSERT_FUNCTION_RETURNS_TRUE(external_sieve_test(2, trace));
    // ASSERT_FUNCTION_RETURNS_TRUE(external_sieve_test(1 << 10, trace));
    // ASSERT_FUNCTION_RETURNS_TRUE(external_sieve_test(1 << 11, trace));
    // ASSERT_FUNCTION_RETURNS_TRUE(external_sieve_test(1 << 12, trace));
    // ASSERT_FUNCTION_RETURNS_TRUE(external_sieve_test(1 << 13, trace));
    // ASSERT_FUNCTION_RETURNS_TRUE(external_sieve_test(1 << 14, trace));
    // ASSERT_FUNCTION_RETURNS_TRUE(external_sieve_test(1 << 15, trace));
    return 0;
}
