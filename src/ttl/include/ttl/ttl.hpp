#pragma once

#include <cstdint>
#include <limits>

#include "math/saturation_arithmetic.h"

// This is to prevent the object from expiring.
constexpr std::size_t FOREVER = std::numeric_limits<std::size_t>::max();

static inline std::uint64_t
get_expiration_time(std::uint64_t const access_time_ms,
                    std::uint64_t const ttl_s)
{
    return saturation_add(access_time_ms, saturation_multiply(1000, ttl_s));
}
