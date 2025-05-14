#pragma once

#include "cpp_lib/format_measurement.hpp"
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

constexpr static uint64_t HOUR_IN_MS = 3600 * 1000;

/// @todo   Configure the start time.
class TemporalSampler {
public:
    TemporalSampler(uint64_t const sampling_period_ms = HOUR_IN_MS)
        : sampling_period_ms_(sampling_period_ms),
          last_sampled_time_ms_(std::nullopt)
    {
    }

    bool
    should_sample(uint64_t const current_time_ms)
    {
        // Sample the first element.
        if (!last_sampled_time_ms_.has_value()) {
            last_sampled_time_ms_ = current_time_ms;
            return true;
        }
        uint64_t const prev_tm = last_sampled_time_ms_.value();
        if (current_time_ms >= prev_tm + sampling_period_ms_) {
            last_sampled_time_ms_ = current_time_ms;
            return true;
        }
        return false;
    }

    std::string
    json() const
    {
        std::stringstream ss;
        ss << "{";
        ss << "\".type\": \"TemporalSampler\", ";
        ss << "\"Sampling Period [ms]\": " << format_time(sampling_period_ms_)
           << ", ";
        ss << "\"Last Sampled Time [ms]\": "
           << format_time(last_sampled_time_ms_.value_or(0));
        ss << "}";
        return ss.str();
    }

private:
    uint64_t const sampling_period_ms_;
    std::optional<uint64_t> last_sampled_time_ms_;
};
