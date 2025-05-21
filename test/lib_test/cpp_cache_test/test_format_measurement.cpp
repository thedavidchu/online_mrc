#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "cpp_lib/format_measurement.hpp"
#include "logger/logger.h"

bool
test_format_underscore()
{
    bool ok = true;
    // Test exhaustively under 1000.
    for (uint64_t i = 0; i < 1000; ++i) {
        if (format_underscore(i) != std::to_string(i)) {
            std::cout << format_underscore(i) << " vs " << std::to_string(i)
                      << std::endl;
            ok = false;
        }
    }

    // Test selectively above 1000.
    std::vector<std::pair<uint64_t, std::string>> oracle = {
        {1000ULL, "1_000"},
        {10000ULL, "10_000"},
        {100000ULL, "100_000"},
        {1000000ULL, "1_000_000"},
        {10000000ULL, "10_000_000"},
        {100000000ULL, "100_000_000"},
        {1000000000ULL, "1_000_000_000"},
        {10000000000ULL, "10_000_000_000"},
        {100000000000ULL, "100_000_000_000"},
        {1000000000000ULL, "1_000_000_000_000"},
        {10000000000000ULL, "10_000_000_000_000"},
        {100000000000000ULL, "100_000_000_000_000"},
        {1000000000000000ULL, "1_000_000_000_000_000"},
        {10000000000000000ULL, "10_000_000_000_000_000"},
        {100000000000000000ULL, "100_000_000_000_000_000"},
        {1000000000000000000ULL, "1_000_000_000_000_000_000"},
        {10000000000000000000ULL, "10_000_000_000_000_000_000"},
        {UINT64_MAX, "18_446_744_073_709_551_615"},
    };
    for (auto [a, b] : oracle) {
        if (format_underscore(a) != b) {
            std::cout << format_underscore(a) << " vs " << b << std::endl;
            ok = false;
        }
    }
    return ok;
}

bool
test_format_binary()
{
    std::map<uint64_t, std::string> map{
        {0, "0b" + std::string(64, '0')},
        {1, "0b" + std::string(63, '0') + "1"},
        {2, "0b" + std::string(62, '0') + "10"},
        {3, "0b" + std::string(62, '0') + "11"},
        {UINT64_MAX, "0b" + std::string(64, '1')},
    };
    for (auto [x, str] : map) {
        if (format_binary(x) != str) {
            LOGGER_DEBUG("for %zu: got %s, expected %s",
                         x,
                         format_binary(x).c_str(),
                         str.c_str());
            return false;
        }
    }
    return true;
}

int
main()
{
    LOGGER_LEVEL = LOGGER_LEVEL_DEBUG;
    bool r = false;
    r = test_format_underscore();
    assert(r);
    r = test_format_binary();
    assert(r);
    return 0;
}
