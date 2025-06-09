#pragma once

#include "cpp_lib/format_measurement.hpp"
#include "cpp_lib/util.hpp"
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

/// @brief  Sample no more than once every time interval.
/// @note   The default (hard-coded) time interval is 1 hour
///         (denominated in milliseconds), but if we have time intervals
///         separated by 2 hours, it will simply skip the hour.
///         Additionally, if we don't sample right on the hour, but a
///         little after, then the next sample time will be based on an
///         hour after this later time.
class TemporalSampler {
private:
    struct UpdateRecommendation {
        bool should_sample;
        bool should_update_time;
    };

    UpdateRecommendation
    check_should_sample(uint64_t const current_time_ms) const
    {
        if (!last_recorded_time_ms_.has_value()) {
            return {should_sample_first_, true};
        }
        uint64_t const prev_tm = last_recorded_time_ms_.value();
        bool r = current_time_ms >= prev_tm + sampling_period_ms_;
        return {r, r};
    }

public:
    static constexpr uint64_t HOUR_IN_MS = 3600 * 1000;

    TemporalSampler(uint64_t const sampling_period_ms = HOUR_IN_MS,
                    bool const should_sample_first = false)
        : sampling_period_ms_(sampling_period_ms),
          should_sample_first_(should_sample_first),
          last_recorded_time_ms_(std::nullopt)
    {
    }

    bool
    should_sample(uint64_t const current_time_ms)
    {
        auto [should_sample, update_time] =
            check_should_sample(current_time_ms);
        if (should_sample) {
            nr_samples_ += 1;
        }
        if (update_time) {
            last_recorded_time_ms_ = current_time_ms;
        }
        return should_sample;
    }

    std::string
    json() const
    {
        std::stringstream ss;
        ss << "{";
        ss << "\".type\": \"TemporalSampler\", ";
        ss << "\"Sampling Period [ms]\": " << format_time(sampling_period_ms_)
           << ", ";
        ss << "\"Should Sample First\": " << bool2str(should_sample_first_)
           << ", ";
        ss << "\"Last Sampled Time [ms]\": "
           << format_time(last_recorded_time_ms_.value_or(0)) << ", ";
        ss << "\"Samples [#]\": " << format_engineering(nr_samples_);
        ss << "}";
        return ss.str();
    }

    uint64_t
    nr_samples() const
    {
        return nr_samples_;
    }

private:
    uint64_t const sampling_period_ms_;
    bool const should_sample_first_;
    std::optional<uint64_t> last_recorded_time_ms_;
    uint64_t nr_samples_ = 0;
};
