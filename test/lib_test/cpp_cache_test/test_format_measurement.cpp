#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "cpp_cache/format_measurement.hpp"

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

int
main()
{
    bool r = false;
    r = test_format_underscore();
    assert(r);
    return 0;
}
