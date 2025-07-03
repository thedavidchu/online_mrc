#pragma once

/** @brief  [TODO]
 *
 *  @todo   Change thresholds to double precision floats.
 **/

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

#include "cpp_lib/histogram.hpp"
#include "cpp_lib/temporal_sampler.hpp"
#include "math/doubles_are_equal.h"

using uint64_t = std::uint64_t;

class LifeTimeThresholds {
    /// @note   This is a relatively expensive function that I removed
    ///         the optimization for because it is only called a small
    ///         number of times.
    std::pair<double, double>
    recalculate_thresholds() const
    {
        double lower = NAN, upper = NAN;

        // We handle the case of 1.0 explicitly, we would simply set the
        // upper_threshold_ to the largest lifespan we have seen yet,
        // rather than the maximum possible.
        if (lower_ratio_ == 0.0) {
            lower = 0;
        } else if (lower_ratio_ == 1.0) {
            lower = INFINITY;
        } else {
            lower = histogram_.lower_bound_percentile(lower_ratio_);
        }
        if (upper_ratio_ == 0.0) {
            upper = 0;
        } else if (upper_ratio_ == 1.0) {
            upper = INFINITY;
        } else {
            upper = histogram_.percentile(upper_ratio_);
        }

        if (std::isnan(lower) || std::isnan(upper)) {
            // TODO: Figure out something smarter to do!
            if (lower_ratio_ == upper_ratio_) {
                return {INFINITY, INFINITY};
            }
            // NOTE DEAD CODE BELOW!
            // This would extend the 'training' period until we see an
            // eviction.
            return {0, INFINITY};
        }
        // If the ratios are the same, then we simply return the mean.
        if (lower_ratio_ == upper_ratio_) {
            double mean = (lower + upper) / 2;
            return {mean, mean};
        }
        return {lower, upper};
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
        if (training_period_) {
            training_period_ = false;
            return true;
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
    ///         the defaults (0, INFINITY).
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
            lower_threshold_ = INFINITY;
            upper_threshold_ = INFINITY;
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
        histogram_.update(lifetime);

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
    std::pair<double, double>
    thresholds() const
    {
        return {lower_threshold_, upper_threshold_};
    }

    std::pair<double, double>
    get_updated_thresholds(uint64_t const current_time_ms)
    {
        if (should_refresh(current_time_ms)) {
            refresh_thresholds();
        }
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
        return histogram_.total();
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

    bool
    has_real_data() const
    {
        return has_real_data_;
    }

    void
    set_real_data()
    {
        has_real_data_ = true;
    }

    std::string
    json() const
    {
        std::stringstream ss;
        ss << "{";
        ss << "\"Histogram\": " << histogram_.json() << ", ";
        ss << "\"Coarse Histogram\": [" << std::get<0>(coarse_histogram_)
           << ", " << std::get<1>(coarse_histogram_) << ", "
           << std::get<2>(coarse_histogram_) << "]";
        ss << ", \"Threshold Refreshes [#]\": "
           << format_engineering(refreshes());
        ss << ", \"Samples Since Threshold Refresh [#]\": "
           << format_engineering(since_refresh());
        ss << ", \"LRU Lifetime Evictions [#]\": "
           << format_engineering(evictions());
        ss << "}";
        return ss.str();
    }

private:
    double const lower_ratio_;
    double const upper_ratio_;
    double lower_threshold_ = 0.0;
    double upper_threshold_ = INFINITY;

    bool training_period_ = true;
    bool has_real_data_ = false;

    Histogram histogram_;
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
