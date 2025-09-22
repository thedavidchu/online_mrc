#pragma once
#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/duration.hpp"
#include "logger/logger.h"
#include <cstdint>

/// @brief  Clean a trace from illegal behaviour, e.g. stepping backward in
///         time or too far forward in time.
/// @note   In theory, this class could save up accesses made in the future and
///         replay them at the appropriate time.
class TraceCleaner {
public:
    TraceCleaner(uint64_t const max_jump = Duration::SECOND,
                 uint64_t const starting_time_ms = 0)
        : max_jump_{max_jump},
          previous_time_ms_{starting_time_ms}
    {
    }
    /// @brief  Whether to use a certain trace entry. Use this before the SHARDS
    ///         sampler, otherwise the jump sizes are going to be much larger.
    bool
    sample(CacheAccess const &access)
    {
        uint64_t const current_time_ms = access.timestamp_ms;
        // We don't allow steps backward in time. This is unambiguously illegal.
        // This is unfortunately fairly common, so we don't warn about it.
        // e.g. 600k examples in Sari's cluster 7.
        if (current_time_ms < previous_time_ms_) {
            return false;
        }
        // We don't allow massive jumps forward either. We can warn because this
        // is presumably rare behaviour but it could theoretically happen, it's
        // just unlikely.
        if (current_time_ms > max_jump_ + previous_time_ms_) {
            LOGGER_WARN("too large of a time jump from %zu to %zu (diff: %zu, "
                        "max diff: %zu)",
                        previous_time_ms_,
                        current_time_ms,
                        current_time_ms - previous_time_ms_,
                        max_jump_);
            return false;
        }
        previous_time_ms_ = current_time_ms;
        return true;
    }

private:
    uint64_t const max_jump_ = Duration::HOUR;
    uint64_t previous_time_ms_ = 0;
};
