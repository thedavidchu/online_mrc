#include "cpp_cache/cache_metadata.hpp"
#include "cpp_cache/cache_access.hpp"

#include "math/saturation_arithmetic.h"

#include <cstdint>
#include <optional>
#include <ostream>

/// @brief  Initialize metadata for unit-sized value size.
CacheMetadata::CacheMetadata(std::uint64_t const insertion_time_ms,
                             std::uint64_t const expiration_time_ms)
    : insertion_time_ms_(insertion_time_ms),
      last_access_time_ms_(insertion_time_ms),
      expiration_time_ms_(expiration_time_ms),
      visited(false)
{
}

/// @brief  Initialize metadata for variable-sized value.
CacheMetadata::CacheMetadata(std::size_t const value_size,
                             std::uint64_t const insertion_time_ms,
                             std::uint64_t const expiration_time_ms)
    : size_(value_size),
      insertion_time_ms_(insertion_time_ms),
      last_access_time_ms_(insertion_time_ms),
      expiration_time_ms_(expiration_time_ms),
      visited(false)
{
}

CacheMetadata::CacheMetadata(CacheAccess const &access)
    : size_(access.size_bytes),
      insertion_time_ms_(access.timestamp_ms),
      last_access_time_ms_(access.timestamp_ms),
      expiration_time_ms_(saturation_add(access.timestamp_ms,
                                         access.ttl_ms.value_or(UINT64_MAX))),
      visited(false)
{
}

template <class Stream>
void
CacheMetadata::to_stream(Stream &s, bool const newline) const
{
    s << "CacheMetadata(frequency=" << frequency_ << ",";
    s << "insertion_time[ms]=" << insertion_time_ms_ << ",";
    s << "last_access_time[ms]=" << last_access_time_ms_ << ",";
    s << "expiration_time[ms]=" << expiration_time_ms_ << ",";
    s << "visited=" << visited << ")";
    if (newline) {
        s << std::endl;
    }
}

void
CacheMetadata::visit(std::uint64_t const access_time_ms,
                     std::optional<std::uint64_t> const new_expiration_time_ms)
{
    ++frequency_;
    last_access_time_ms_ = access_time_ms;
    if (new_expiration_time_ms) {
        expiration_time_ms_ = new_expiration_time_ms.value();
    }
}

void
CacheMetadata::unvisit()
{
    visited = false;
}

/// @brief  Get the TTL from the last access.
uint64_t
CacheMetadata::ttl_ms() const
{
    return expiration_time_ms_ - last_access_time_ms_;
}
