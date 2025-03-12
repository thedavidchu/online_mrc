/** @brief  [TODO]
 *
 *  @todo   Support different lower/upper uncertainties.
 **/
#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <tuple>
#include <utility>

#include "math/doubles_are_equal.h"

using size_t = std::size_t;
using uint64_t = std::uint64_t;

class LifeTimeThresholds {
private:
    std::pair<uint64_t, uint64_t>
    recalculate_thresholds() const
    {
        assert(uncertainty_ >= 0.0 && uncertainty_ <= 0.5);

        uint64_t lower = 0, upper = std::numeric_limits<uint64_t>::max();
        uint64_t accum = 0;
        uint64_t prev_lifetime = 0;
        bool found_lower = false;
        // Find the (50{+,-}uncertainty)th percentile lifetimes.
        for (auto [lifetime, frq] : histogram_) {
            accum += frq;
            if (!found_lower && (double)accum / total_ > 0.5 - uncertainty_) {
                lower = prev_lifetime;
                found_lower = true;
            }
            if ((double)accum / total_ >= 0.5 + uncertainty_) {
                upper = lifetime;
                break;
            }
            prev_lifetime = lifetime;
        }

        return {lower, upper};
    }

    /// @brief  Return if the thresholds should be refreshed.
    /// @note   I allow them to be 1% out of alignment. How did I choose 1%? I
    /// just did, that's how.
    bool
    should_refresh() const
    {
        // In future, I may support different uncertainties for below
        // and above the median.
        double const lower_uncertainty = uncertainty_;
        double const upper_uncertainty = uncertainty_;
        double const error = 0.01 * coarse_counter_;

        // We don't readjust until we have at least 1 million data points!
        // TODO - consider putting a time bound on this.
        if (coarse_counter_ < MIN_COARSE_COUNT) {
            return false;
        }

        // The expected count of the lower
        double const low_cnt = (0.5 - lower_uncertainty) * coarse_counter_;
        double const mid_cnt =
            (lower_uncertainty + upper_uncertainty) * coarse_counter_;
        double const up_cnt = (0.5 - upper_uncertainty) * coarse_counter_;

        return !(
            doubles_are_close(low_cnt, std::get<0>(coarse_histogram_), error) &&
            doubles_are_close(mid_cnt, std::get<1>(coarse_histogram_), error) &&
            doubles_are_close(up_cnt, std::get<2>(coarse_histogram_), error));
    }

public:
    LifeTimeThresholds(double const uncertainty)
        : uncertainty_(uncertainty),
          coarse_histogram_({0, 0, 0})
    {
        assert(uncertainty >= 0.0 && uncertainty <= 0.5);
    }

    void
    register_cache_eviction(uint64_t const lifetime, uint64_t const size)
    {
        // We are counting objects for now.
        // TODO Count bytes in the future, maybe?
        (void)size;
        total_ += 1;
        histogram_[lifetime] += 1;

        if (lower_threshold > lifetime) {
            ++std::get<0>(coarse_histogram_);
            ++coarse_counter_;
        } else if (upper_threshold > lifetime) {
            ++std::get<1>(coarse_histogram_);
            ++coarse_counter_;
        } else {
            ++std::get<2>(coarse_histogram_);
            ++coarse_counter_;
        }

        if (should_refresh()) {
            refresh_thresholds();
        }
    }

    void
    refresh_thresholds()
    {
        auto x = recalculate_thresholds();
        lower_threshold = x.first;
        upper_threshold = x.second;
        coarse_histogram_ = {0, 0, 0};
        coarse_counter_ = 0;
        ++num_refresh_;
    }

    /// @note   Automatically refresh the thresholds when there's a mismatch.
    std::pair<uint64_t, uint64_t>
    thresholds() const
    {
        return {lower_threshold, upper_threshold};
    }

    size_t
    refreshes() const
    {
        return num_refresh_;
    }

    size_t
    evictions() const
    {
        return total_;
    }

    size_t
    since_refresh() const
    {
        return coarse_counter_;
    }

private:
    double const uncertainty_;
    uint64_t lower_threshold = 0;
    uint64_t upper_threshold = std::numeric_limits<uint64_t>::max();

    uint64_t total_ = 0;
    std::map<uint64_t, uint64_t> histogram_;
    // This tells us how far off our current estimate of the thresholds
    // may possibly be.
    std::tuple<uint64_t, uint64_t, uint64_t> coarse_histogram_;
    size_t coarse_counter_ = 0;
    size_t num_refresh_ = 0;

public:
    // TODO Make this configurable?
    static constexpr size_t MIN_COARSE_COUNT = 1 << 20;
};
