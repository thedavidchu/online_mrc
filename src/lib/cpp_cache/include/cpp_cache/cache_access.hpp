/** @brief  Represent a cache access. */
#pragma once
#include <cassert>
#include <cstdint>
#include <optional>

#include "trace/trace.h"

using uint64_t = std::uint64_t;
using uint32_t = std::uint32_t;
using uint8_t = std::uint8_t;

enum class CacheAccessCommand : uint8_t {
    get,
    set,
};

struct CacheAccess {
    uint64_t const timestamp_ms;
    CacheAccessCommand const command;
    uint64_t const key;
    uint32_t const size_bytes;
    std::optional<uint64_t> const ttl_ms;

    /// @brief  Initialize a CacheAccess object from a FullTraceItem.
    CacheAccess(struct FullTraceItem const *const item);

    CacheAccess(uint64_t const timestamp_ms, uint64_t const key);

    CacheAccess(uint64_t const timestamp_ms,
                uint64_t const key,
                uint64_t size_bytes,
                std::optional<uint64_t> ttl_ms);

    std::optional<uint64_t>
    expiration_time_ms() const;
};
