#pragma once

#include <algorithm>
#include <cassert>
#include <cstdbool>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "arrays/array_size.h"
#include "arrays/is_last.h"

// It would be more complicated to support larger values with exponents
// larger than 2^64, because that exceeds the maximum of a uint64.
std::string const SI_PREFIX_STRINGS[] =
    {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};

static inline std::string
format_memory_size(double const size_bytes)
{
    size_t i = 0;
    for (i = 0; i < ARRAY_SIZE(SI_PREFIX_STRINGS); ++i) {
        if (size_bytes / ((uint64_t)1 << (i * 10)) < 1024) {
            break;
        }
    }
    // This would in theory return exbibytes for anything larger than
    // an EiB, but due to the limits of size_t, this is not possible.
    if (i >= ARRAY_SIZE(SI_PREFIX_STRINGS)) {
        i = ARRAY_SIZE(SI_PREFIX_STRINGS) - 1;
    }
    // NOTE I formatted this for compatibility with C and C++. I dislike
    //      many elements of C++, like the lack of designated initializers!
    return "\"" +
           std::to_string((double)size_bytes / ((uint64_t)1 << (i * 10))) +
           " " + SI_PREFIX_STRINGS[i] + "\"";
}

/// @note   I do this in time units that I personally find useful:
///         milliseconds, seconds, minutes, hours, days, and years.
///         I conspicuously don't support weeks or months, because those
///         aren't as nice to think of.
/// @note   I rely on 'double' having 52 bits of precision, which should
//          mean that most realistic times are not truncated.
static inline std::string
format_time(double const time_ms)
{
    // NOTE I need to use a 'double' here (hence the trailing '.0')
    //      because the number of ms/year exceeds the size of 'int'.
    if (time_ms >= 1000.0 * 3600 * 24 * 365) {
        return "\"" + std::to_string(time_ms / (1000.0 * 3600 * 24 * 365)) +
               " year\"";
    }
    // NOTE I can use regular 'int' in all the below cases.
    if (time_ms >= 1000 * 3600 * 24) {
        return "\"" + std::to_string(time_ms / (1000 * 3600 * 24)) + " day\"";
    }
    if (time_ms >= 1000 * 3660) {
        return "\"" + std::to_string(time_ms / (1000 * 3600)) + " h\"";
    }
    if (time_ms >= 1000 * 60) {
        return "\"" + std::to_string(time_ms / (1000 * 60)) + " min\"";
    }
    if (time_ms >= 1000) {
        return "\"" + std::to_string(time_ms / 1000) + " s\"";
    }
    return "\"" + std::to_string(time_ms) + " ms\"";
}

static inline std::string
format_engineering(double const value)
{
    double mantissa = value;
    size_t exp10 = 0;
    while (mantissa > 1000) {
        mantissa /= 1000;
        exp10 += 3;
    }

    if (exp10 == 0) {
        return std::to_string(mantissa);
    }
    return std::to_string(mantissa) + "e" + std::to_string(exp10);
}

static inline std::string
format_underscore(uint64_t const value)
{
    std::vector<char> digits;
    std::string original_string = std::to_string(value);
    std::reverse(original_string.begin(), original_string.end());
    for (size_t i = 0; i < original_string.size(); ++i) {
        char c = original_string.at(i);
        digits.push_back(c);
        if (!is_last(i, original_string.size()) && i % 3 == 2) {
            digits.push_back('_');
        }
    }
    std::reverse(digits.begin(), digits.end());
    std::string str{digits.begin(), digits.end()};
    return str;
}

static inline std::string
format_percent(double const ratio)
{
    return std::to_string(100 * ratio) + "%";
}
