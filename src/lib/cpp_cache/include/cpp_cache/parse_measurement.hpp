#pragma once

#include "logger/logger.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <unordered_map>

using uint64_t = std::uint64_t;

static const std::unordered_map<std::string, uint64_t> mem_units = {
    {"B", (uint64_t)1},
    {"kB", (uint64_t)1e3},
    {"MB", (uint64_t)1e6},
    {"GB", (uint64_t)1e9},
    {"TB", (uint64_t)1e12},
    {"PB", (uint64_t)1e18},
    {"EB", (uint64_t)1e21},
    {"KiB", (uint64_t)1 << 10},
    {"MiB", (uint64_t)1 << 20},
    {"GiB", (uint64_t)1 << 30},
    {"TiB", (uint64_t)1 << 40},
    {"PiB", (uint64_t)1 << 50},
    {"EiB", (uint64_t)1 << 60},
};

/// @brief  Parse memory size of form '100MiB'.
static inline std::optional<uint64_t>
parse_memory_size(std::string const &str)
{
    char const *str_beg = str.c_str();
    char *str_end = nullptr;
    uint64_t ans = std::strtoll(str_beg, &str_end, 10);
    if (ans == 0 && str_beg == str_end) {
        LOGGER_ERROR("cannot parse '%s' as memory size", str.c_str());
        return {};
    }
    std::string unit_str = std::string(str_end);
    if (!mem_units.count(unit_str)) {
        LOGGER_ERROR("cannot parse '%s' as memory unit", unit_str.c_str());
        return {};
    }
    return ans * mem_units.at(unit_str);
}
