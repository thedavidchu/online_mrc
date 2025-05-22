#pragma once

/** @brief  [TODO]
 *
 *  @todo   Change thresholds to double precision floats.
 **/

#include <cassert>
#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

#include "cpp_lib/temporal_sampler.hpp"
#include "cpp_lib/util.hpp"
#include "logger/logger.h"
#include "math/doubles_are_equal.h"

using uint64_t = std::uint64_t;

class LifeTimeThresholds {
    std::pair<uint64_t, uint64_t>
    recalculate_thresholds() const
    {
        std::optional<uint64_t> lower = std::nullopt, upper = std::nullopt;
        uint64_t accum = 0, prev_lifetime = 0;

        // Handle special cases. If we didn't handle the case of 1.0
        // explicitly, we would simply set the upper_threshold_ to the
        // largest lifespan we have seen yet, rather than the maximum
        // possible.
        if (lower_ratio_ == 0.0) {
            lower = 0;
        } else if (lower_ratio_ == 1.0) {
            lower = UINT64_MAX;
        }
        if (upper_ratio_ == 0.0) {
            upper = 0;
        } else if (upper_ratio_ == 1.0) {
            upper = UINT64_MAX;
        }

        for (auto [lifetime, frq] : histogram_) {
            // Do this first in case both lower and upper are already set.
            if (lower.has_value() && upper.has_value()) {
                break;
            }
            accum += frq;
            if (!lower.has_value() && (double)accum / total_ > lower_ratio_) {
                lower = prev_lifetime;
            }
            if (!upper.has_value() && (double)accum / total_ >= upper_ratio_) {
                upper = lifetime;
            }
            prev_lifetime = lifetime;
        }

        if (!lower.has_value() || !upper.has_value()) {
            LOGGER_WARN("lower or upper does not have value, so defaulting to "
                        "original (0, UINT64_MAX)");
            return {0, UINT64_MAX};
        }
        // If the ratios are the same, then we simply return the mean.
        // This does bias us to round down, however. It may be a bit of
        // an optimization to simply return the lower threshold in this
        // case, because we compute it earlier than the upper threshold.
        if (lower_ratio_ == upper_ratio_) {
            double mean = (double)(lower.value() + upper.value()) / 2;
            return {mean, mean};
        }
        return {lower.value(), upper.value()};
    }

    /// @brief  Return if the thresholds should be refreshed.
    /// @note   I allow them to be 1% out of alignment. How did I choose 1%? I
    /// just did, that's how.
    bool
    should_refresh(uint64_t const current_time_ms)
    {
        double const error = 0.01 * coarse_counter_;

        // NOTE An update may occur any time after a hour past the last
        //      sample once the coarse histogram is off by the error
        //      margin. This is as opposed to checking once per hour
        //      whether both are true.
        if (!refresher_.should_sample(current_time_ms)) {
            return false;
        }

        double const low_cnt = lower_ratio_ * coarse_counter_;
        double const mid_cnt = (upper_ratio_ - lower_ratio_) * coarse_counter_;
        double const up_cnt = (1.0 - upper_ratio_) * coarse_counter_;

        return !(
            doubles_are_close(low_cnt, std::get<0>(coarse_histogram_), error) &&
            doubles_are_close(mid_cnt, std::get<1>(coarse_histogram_), error) &&
            doubles_are_close(up_cnt, std::get<2>(coarse_histogram_), error));
    }

public:
    /// @todo   Allow setting the starting time. This is more for
    ///         redundancy to make sure everything is safe.
    ///         This isn't strictly necessary because we refresh the
    ///         thresholds before we have any data, we just set it to
    ///         the defaults (0, UINT64_MAX).
    LifeTimeThresholds(double const lower_ratio, double const upper_ratio)
        : lower_ratio_(lower_ratio),
          upper_ratio_(upper_ratio),
          coarse_histogram_({0, 0, 0})
    {
        assert(lower_ratio >= 0.0 && lower_ratio <= 1.0);
        assert(upper_ratio >= 0.0 && upper_ratio <= 1.0);
        assert(lower_ratio <= upper_ratio);

        if (lower_ratio == 0 && upper_ratio == 0) {
            lower_threshold_ = 0;
            upper_threshold_ = 0;
        }
        if (lower_ratio == 1.0 && upper_ratio == 1.0) {
            lower_threshold_ = UINT64_MAX;
            upper_threshold_ = UINT64_MAX;
        }
    }

    void
    register_cache_eviction(uint64_t const lifetime,
                            uint64_t const size,
                            uint64_t const current_time_ms)
    {
        // We are counting objects for now.
        // TODO Count bytes in the future, maybe?
        (void)size;
        total_ += 1;
        histogram_[lifetime] += 1;

        if (lower_threshold_ > lifetime) {
            ++std::get<0>(coarse_histogram_);
            ++coarse_counter_;
        } else if (upper_threshold_ > lifetime) {
            ++std::get<1>(coarse_histogram_);
            ++coarse_counter_;
        } else {
            ++std::get<2>(coarse_histogram_);
            ++coarse_counter_;
        }

        if (should_refresh(current_time_ms)) {
            refresh_thresholds();
        }
    }

    void
    refresh_thresholds()
    {
        auto x = recalculate_thresholds();
        lower_threshold_ = x.first;
        upper_threshold_ = x.second;
        coarse_histogram_ = {0, 0, 0};
        coarse_counter_ = 0;
    }

    /// @note   Automatically refresh the thresholds when there's a mismatch.
    std::pair<uint64_t, uint64_t>
    thresholds() const
    {
        return {lower_threshold_, upper_threshold_};
    }

    uint64_t
    refreshes() const
    {
        return refresher_.nr_samples();
    }

    uint64_t
    evictions() const
    {
        return total_;
    }

    uint64_t
    since_refresh() const
    {
        return coarse_counter_;
    }

    double
    lower_ratio() const
    {
        return lower_ratio_;
    }

    double
    upper_ratio() const
    {
        return upper_ratio_;
    }

    std::string
    json() const
    {
        std::stringstream ss;
        ss << "{";
        ss << "\"Histogram\": " << map2str(histogram_) << ", ";
        ss << "\"Coarse Histogram\": [" << std::get<0>(coarse_histogram_)
           << ", " << std::get<1>(coarse_histogram_) << ", "
           << std::get<2>(coarse_histogram_) << "]";
        ss << "}";
        return ss.str();
    }

private:
    double const lower_ratio_;
    double const upper_ratio_;
    uint64_t lower_threshold_ = 0;
    uint64_t upper_threshold_ = UINT64_MAX;

    uint64_t total_ = 0;
    std::map<uint64_t, uint64_t> histogram_;
    // This tells us how far off our current estimate of the thresholds
    // may possibly be.
    std::tuple<uint64_t, uint64_t, uint64_t> coarse_histogram_;
    // The previous timestamp at which we refreshed.
    TemporalSampler refresher_{MIN_TIME_DELTA, false};
    uint64_t coarse_counter_ = 0;

public:
    // TODO Make this configurable?
    static constexpr uint64_t MIN_TIME_DELTA = 1000 * 3600;
};
