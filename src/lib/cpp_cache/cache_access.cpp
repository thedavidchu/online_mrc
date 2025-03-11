#include <cassert>
#include <cstdint>
#include <optional>

#include "cpp_cache/cache_access.hpp"

#include "math/saturation_arithmetic.h"
#include "trace/trace.h"

/// @brief  Initialize a CacheAccess object from a FullTraceItem.
CacheAccess::CacheAccess(struct FullTraceItem const *const item)
    : timestamp_ms(item->timestamp_ms),
      command((CacheAccessCommand)item->command),
      key(item->key),
      size_bytes(item->size),
      // A TTL of 0 in Kia's traces implies no TTL. I assume it's
      // the same in Sari's, but I don't know.
      ttl_ms(item->ttl_s == 0
                 ? std::nullopt
                 : std::optional(saturation_multiply(1000, item->ttl_s)))
{
}

CacheAccess::CacheAccess(std::uint64_t const timestamp_ms,
                         std::uint64_t const key)
    : timestamp_ms(timestamp_ms),
      command(CacheAccessCommand::get),
      key(key),
      size_bytes(1),
      ttl_ms(std::nullopt)
{
}

CacheAccess::CacheAccess(std::uint64_t const timestamp_ms,
                         std::uint64_t const key,
                         std::uint64_t size_bytes,
                         std::optional<std::uint64_t> ttl_ms)
    : timestamp_ms(timestamp_ms),
      command(CacheAccessCommand::get),
      key(key),
      size_bytes(size_bytes),
      ttl_ms(ttl_ms)
{
}
