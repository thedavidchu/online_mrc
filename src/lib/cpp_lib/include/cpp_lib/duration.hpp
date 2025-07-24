/// @brief  A class for durations.
/// @note   Yes, I know C++ has a library (chrono) for this, but I got stuck
///         with this due to legacy C stuff and gradual laziness. This is a
///         simplified model of Go's duration, which I appreciate.
#pragma once

#include <cstdint>

using uint64_t = std::uint64_t;

namespace Duration {
constexpr uint64_t MS = 1;
constexpr uint64_t SECOND = 1000 * MS;
constexpr uint64_t MINUTE = 60 * SECOND;
constexpr uint64_t HOUR = 60 * MINUTE;
constexpr uint64_t DAY = 24 * HOUR;
constexpr uint64_t WEEK = 7 * DAY;
// Obviously, this doesn't support leap years.
constexpr uint64_t YEAR = 365 * DAY;
} // namespace Duration
