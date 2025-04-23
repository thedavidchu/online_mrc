/** @brief  Represent a cache access. */
#pragma once
#include <cassert>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

#include "cpp_lib/cache_command.hpp"
#include "cpp_lib/cache_trace_format.hpp"
#include "trace/trace.h"

using uint64_t = std::uint64_t;
using uint32_t = std::uint32_t;
using uint8_t = std::uint8_t;

struct CacheAccess {
    uint64_t const timestamp_ms = 0;
    CacheCommand const command = CacheCommand::Invalid;
    uint64_t const key = 0;
    uint64_t const key_size_b = 0;
    uint64_t const value_size_b = 0;
    std::optional<uint64_t> const ttl_ms = {};
    uint64_t const client_id = 0;

    /// @brief  Initialize a CacheAccess object from a FullTraceItem.
    CacheAccess(struct FullTraceItem const *const item);

    CacheAccess(uint64_t const timestamp_ms, uint64_t const key);

    CacheAccess(uint64_t const timestamp_ms,
                uint64_t const key,
                uint64_t value_size_b,
                std::optional<uint64_t> ttl_ms);

    CacheAccess(uint8_t const *const ptr, CacheTraceFormat const format);

    bool
    is_read() const;

    bool
    is_write() const;

    std::optional<uint64_t>
    expiration_time_ms() const;

    /// @brief  A wrapper around the hard-to-use ttl_ms.
    double
    time_to_live_ms() const
    {
        if (command != CacheCommand::Get && command != CacheCommand::Gets) {
            double ttl = ttl_ms.value_or(INFINITY);
            if (ttl == 0) {
                ttl = INFINITY;
            }
            return ttl;
        }
        return NAN;
    }

    /// @brief  Return a comma-separated string of the internals.
    ///         Format is the same as the Twitter trace CSVs.
    /// @note   These is NO trailing comma.
    std::string
    twitter_csv(bool const newline = false) const;
};
