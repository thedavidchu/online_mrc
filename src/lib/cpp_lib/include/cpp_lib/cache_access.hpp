/** @brief  Represent a cache access. */
#pragma once
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>

#include "cpp_lib/cache_command.hpp"
#include "cpp_lib/cache_trace_format.hpp"

using uint64_t = std::uint64_t;
using uint32_t = std::uint32_t;
using uint8_t = std::uint8_t;

struct CacheAccess {
    uint64_t const timestamp_ms = 0;
    CacheCommand const command = CacheCommand::Invalid;
    uint64_t const key = 0;
    uint64_t const key_size_b = 0;
    uint64_t const value_size_b = 0;
    double const ttl_ms = NAN;
    uint64_t const client_id = 0;

    CacheAccess(uint64_t const timestamp_ms,
                uint64_t const key,
                uint64_t value_size_b = 1,
                double const ttl_ms = INFINITY);

    CacheAccess(uint8_t const *const ptr, CacheTraceFormat const format);

    bool
    is_read() const;

    bool
    is_write() const;

    bool
    has_ttl() const;

    double
    expiration_time_ms() const;

    /// @brief  Return a comma-separated string of the internals.
    ///         Format is the same as the Twitter trace CSVs.
    /// @note   These is NO trailing comma.
    std::string
    twitter_csv(bool const newline = false) const;
};
