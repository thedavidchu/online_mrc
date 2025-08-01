#pragma once

#include "cpp_lib/format_measurement.hpp"
#include "cpp_lib/util.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>

using uint64_t = std::uint64_t;

class Histogram {
    enum class Bound {
        Lower,
        UpperOrEqual,
    };

    static std::string
    stringify_double(double const x)
    {
        // Print integers exactly; print all else as a float.
        if (std::isnormal(x) && (int64_t)x == x) {
            return std::to_string((int64_t)x);
        }
        return std::to_string(x);
    }

    std::map<double, uint64_t>
    ordered_histogram() const
    {
        std::map<double, uint64_t> ordered_hist;
        for (auto [b, frq] : histogram_) {
            ordered_hist[b] = frq;
        }
        return ordered_hist;
    }

    uint64_t
    count_bucket(double bucket) const
    {
        return histogram_.count(bucket) ? histogram_.at(bucket) : 0.0;
    }

    /// @brief  Duh, this percentile is ordered. I just say it's ordered
    ///         to disambiguate the name from the public method.
    double
    ordered_percentile(double const ratio, Bound const bound) const
    {
        double target = ratio * total_;
        double cnt = 0.0;
        // QUESTION Does this make sense?
        double prev_b = -INFINITY;
        if (histogram_.size() == 0) {
            return NAN;
        }
        for (auto [b, frq] : ordered_histogram()) {
            cnt += frq;
            switch (bound) {
            case Bound::Lower:
                if (cnt > target) {
                    return prev_b;
                }
                prev_b = b;
                break;
            case Bound::UpperOrEqual:
                if (cnt >= target) {
                    return b;
                }
                break;
            }
        }
        return INFINITY;
    }

public:
    /// @brief  Decay the values of the histogram.
    /// @param  alpha: double = 0.5
    ///         The ratio of the old value to keep.
    void
    decay_histogram(double const alpha = 0.5)
    {
        std::vector<uint64_t> victims;
        uint64_t new_total_ = 0;
        for (auto &[tm, frq] : histogram_) {
            frq *= alpha;
            if (frq == 0) {
                victims.push_back(tm);
            }
            new_total_ += frq;
        }
        // We cannot modify the histogram structure as we iterate through it.
        for (auto v : victims) {
            histogram_.erase(v);
        }
        total_ = new_total_;
    }

    /// @note   I just let it overflow...
    void
    update(double const bucket, uint64_t const frq = 1)
    {
        assert(bucket != NAN);
        total_ += frq;
        histogram_[bucket] += frq;
    }

    double
    mean() const
    {
        double accum = 0.0;
        for (auto [b, frq] : histogram_) {
            accum += b * frq;
        }
        return accum / total_;
    }

    /// @brief  Return the bucket with the highest frequency, broken by
    ///         the largest bucket number.
    double
    mode() const
    {
        double mode = NAN;
        uint64_t mode_frq = 0;
        for (auto [b, frq] : histogram_) {
            if (frq >= mode_frq) {
                mode = b;
                mode_frq = frq;
            }
        }
        return mode;
    }

    /// @brief  Get where at least 'ratio' of objects are lesser.
    double
    percentile(double const ratio) const
    {
        return ordered_percentile(ratio, Bound::UpperOrEqual);
    }

    double
    lower_bound_percentile(double const ratio) const
    {
        return ordered_percentile(ratio, Bound::Lower);
    }

    uint64_t
    total() const
    {
        return total_;
    }

    uint64_t
    zero() const
    {
        return count_bucket(0.0);
    }

    uint64_t
    nonzero() const
    {
        return total_ - count_bucket(0.0);
    }

    /// @brief  Return a CSV of the buckets.
    /// @note   This is for human readability, not for minimalism.
    std::string
    csv(std::string sep = ",") const
    {
        std::stringstream ss;
        ss << "Total,Bucket,Frequency,PDF,CDF" << std::endl;
        uint64_t accum = 0;
        for (auto [b, f] : ordered_histogram()) {
            accum += f;
            ss << total_ << sep << stringify_double(b) << sep << f << sep
               << (double)f / total_ << sep << (double)accum / total_
               << std::endl;
        }
        return ss.str();
    }

    /// @brief  Print a time-based histogram.
    void
    print_time(std::string const &name, uint64_t const level = 1) const
    {
        // Add a suitable level of indentation with [repeated] '>'.
        std::string prefix = level ? (std::string(level, '>') + " ") : "";
        prefix += name;
        std::cout << prefix << "Min: " << format_time(percentile(0.0))
                  << std::endl;
        std::cout << prefix << "Q1: " << format_time(percentile(0.25))
                  << std::endl;
        std::cout << prefix << "Median: " << format_time(percentile(0.5))
                  << std::endl;
        std::cout << prefix << "Q3: " << format_time(percentile(0.75))
                  << std::endl;
        std::cout << prefix << "Max: " << format_time(percentile(1.0))
                  << std::endl;
        std::cout << prefix << "Mean: " << format_time(mean()) << std::endl;
        // I named this so verbosely to avoid a name conflict with the function.
        double const maximum_mode = mode();
        std::cout << prefix << "Max Mode: " << format_time(maximum_mode)
                  << " -- "
                  << format_pretty_ratio(count_bucket(maximum_mode), total_)
                  << std::endl;
        std::cout << prefix << "# Zero: " << format_pretty_ratio(zero(), total_)
                  << std::endl;
        std::cout << prefix
                  << "# Non-zero: " << format_pretty_ratio(nonzero(), total_)
                  << std::endl;
        std::cout << prefix << "# TOTAL: " << format_underscore(total_)
                  << std::endl;
    }

    std::string
    json() const
    {
        std::stringstream ss;
        ss << "{";
        ss << "\".type\": \"Histogram\", ";
        ss << "\"total\": " << total_ << ", ";
        ss << "\"histogram\": " << map2str(histogram_);
        ss << "}";
        return ss.str();
    }

private:
    uint64_t total_ = 0;
    std::unordered_map<double, uint64_t> histogram_;
};
