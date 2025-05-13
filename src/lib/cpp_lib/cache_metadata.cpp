#include "cpp_lib/cache_metadata.hpp"
#include "cpp_lib/cache_access.hpp"

#include <cmath>
#include <cstdint>
#include <optional>

/// @brief  Initialize metadata for unit-sized value size.
CacheMetadata::CacheMetadata(std::uint64_t const insertion_time_ms,
                             double const expiration_time_ms)
    : insertion_time_ms_(insertion_time_ms),
      last_access_time_ms_(insertion_time_ms),
      expiration_time_ms_(expiration_time_ms),
      visited(false)
{
}

/// @brief  Initialize metadata for variable-sized value.
CacheMetadata::CacheMetadata(std::size_t const value_size,
                             std::uint64_t const insertion_time_ms,
                             double const expiration_time_ms)
    : size_(value_size),
      insertion_time_ms_(insertion_time_ms),
      last_access_time_ms_(insertion_time_ms),
      expiration_time_ms_(expiration_time_ms),
      visited(false)
{
}

CacheMetadata::CacheMetadata(CacheAccess const &access)
    : size_(access.value_size_b),
      insertion_time_ms_(access.timestamp_ms),
      last_access_time_ms_(access.timestamp_ms),
      expiration_time_ms_(std::isnan(access.ttl_ms)
                              ? NAN
                              : access.timestamp_ms + access.ttl_ms),
      visited(false)
{
}

void
CacheMetadata::visit_without_ttl_refresh(CacheAccess const &access)
{
    ++frequency_;
    last_access_time_ms_ = access.timestamp_ms;
    visited = true;
    size_ = access.value_size_b;
    // NOTE We intentionally don't update the expiration time because
    //      that's how most caches work. Previously, we had allowed
    //      updating it because we needed the update semantics to
    //      replicate LRU with TTLs.
}

void
CacheMetadata::visit(std::uint64_t const access_time_ms,
                     std::optional<double> const new_expiration_time_ms)
{
    ++frequency_;
    last_access_time_ms_ = access_time_ms;
    if (new_expiration_time_ms.has_value()) {
        expiration_time_ms_ = new_expiration_time_ms.value();
    }
}

void
CacheMetadata::unvisit()
{
    visited = false;
}

double
CacheMetadata::ttl_ms(uint64_t const current_time_ms) const
{
    return expiration_time_ms_ - current_time_ms;
}
