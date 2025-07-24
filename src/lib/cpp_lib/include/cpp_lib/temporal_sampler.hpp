#pragma once

#include "cpp_lib/format_measurement.hpp"
#include "cpp_lib/util.hpp"
#include <cassert>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

class TemporalSampler {
private:
    bool
    is_first() const
    {
        assert((first_recorded_time_ms_ && last_recorded_time_ms_) ||
               (!first_recorded_time_ms_ && !last_recorded_time_ms_));
        return !first_recorded_time_ms_ && !last_recorded_time_ms_;
    }

    bool
    p_should_sample_first(uint64_t const current_time_ms)
    {
        assert(is_first());
        first_recorded_time_ms_ = current_time_ms;
        last_recorded_time_ms_ = current_time_ms;
        if (sample_first_) {
            nr_samples_ += 1;
        }
        return sample_first_;
    }

    /// @brief  Should sample, based off the time from the last sample.
    bool
    p_should_sample_since_last(uint64_t const current_time_ms)
    {
        assert(!is_first() && since_last_sample_);
        uint64_t const prev_tm = last_recorded_time_ms_.value();
        if (current_time_ms >= prev_tm + sampling_period_ms_) {
            nr_samples_ += 1;
            last_recorded_time_ms_ = current_time_ms;
            return true;
        }
        return false;
    }

    /// @brief  Should sample, based off the time from the start.
    bool
    p_should_sample_since_first(uint64_t const current_time_ms)
    {
        assert(!is_first() && !since_last_sample_);
        // Round down the previous sampled time to the sampling period
        // it belongs to.
        uint64_t const prev_tm = ((last_recorded_time_ms_.value() -
                                   first_recorded_time_ms_.value()) /
                                  sampling_period_ms_) *
                                     sampling_period_ms_ +
                                 first_recorded_time_ms_.value();
        if (current_time_ms >= prev_tm + sampling_period_ms_) {
            nr_samples_ += 1;
            last_recorded_time_ms_ = current_time_ms;
            return true;
        }
        return false;
    }

public:
    static constexpr uint64_t HOUR_IN_MS = 3600 * 1000;

    /// @brief  Sample no more than once every time interval.
    /// @note   If we have time intervals separated by 2 hours, it
    ///         samples once.
    /// @param sampling_period_ms
    ///     The default time interval is 1 hour (in milliseconds).
    /// @param since_last_sample
    ///     Should the sampling period be since the last sample (as
    ///     opposed to the next expected time slot).
    ///     For example, if we don't sample right on the hour, but a
    ///     little after, then the next sample time will be based on an
    ///     hour after this later time.
    ///     Example
    ///     -------
    ///     Period: 0   1   2   3
    ///     Samples:X    X (X)      (X) is sampled when this is false.
    TemporalSampler(uint64_t const sampling_period_ms = HOUR_IN_MS,
                    bool const sample_first = false,
                    bool const since_last_sample = true)
        : sampling_period_ms_(sampling_period_ms),
          sample_first_(sample_first),
          since_last_sample_(since_last_sample)
    {
    }

    bool
    should_sample(uint64_t const current_time_ms)
    {
        if (is_first()) {
            return p_should_sample_first(current_time_ms);
        } else {
            if (since_last_sample_) {
                return p_should_sample_since_last(current_time_ms);
            } else {
                return p_should_sample_since_first(current_time_ms);
            }
        }
    }

    std::string
    json() const
    {
        std::stringstream ss;
        ss << "{";
        ss << "\".type\": \"TemporalSampler\", ";
        ss << "\"Sampling Period [ms]\": " << format_time(sampling_period_ms_)
           << ", ";
        ss << "\"Sample First\": " << bool2str(sample_first_) << ", ";
        ss << "\"Since Last Sample\": " << bool2str(since_last_sample_) << ", ";
        ss << "\"First Sampled Time [ms]\": "
           << format_time(first_recorded_time_ms_.value_or(0)) << ", ";
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
    bool const sample_first_;
    bool const since_last_sample_;
    std::optional<uint64_t> first_recorded_time_ms_ = std::nullopt;
    // This is the most recent recorded time. We record the time on the
    // first access (even if this is not sampled) and on every
    // subsequent sample.
    std::optional<uint64_t> last_recorded_time_ms_ = std::nullopt;
    uint64_t nr_samples_ = 0;
};
