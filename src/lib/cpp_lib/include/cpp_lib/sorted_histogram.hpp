#pragma once

#include "cpp_lib/util.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>

using uint64_t = std::uint64_t;

class SortedHistogram {
    double
    percentile(double const ratio, bool const inclusive) const
    {
        assert(0 &&
               "untested and non-inclusive mode may not make logical sense but "
               "I copied it from another the lifetime thresholds");
        double target = ratio * total_;
        double cnt = 0.0;
        // QUESTION Does this make sense?
        double prev_b = -INFINITY;
        if (histogram_.size() == 0) {
            return NAN;
        }
        for (auto [b, frq] : histogram_) {
            cnt += frq;
            if (inclusive) {
                if (cnt >= target) {
                    return b;
                }
            } else {
                if (cnt > target) {
                    return prev_b;
                }
                prev_b = b;
            }
        }
        return INFINITY;
    }

public:
    void
    update(double const bucket, uint64_t const frq = 1)
    {
        assert(bucket != NAN);
        total_ += frq;
        histogram_[bucket] += frq;
    }

    double
    exclusive_percentile(double const ratio) const
    {
        assert(0 && "untested and logic may not be correct!");
        return percentile(ratio, false);
    }

    double
    inclusive_percentile(double const ratio) const
    {
        assert(0 && "untested!");
        return percentile(ratio, true);
    }

    uint64_t
    total() const
    {
        return total_;
    }

    auto
    begin() const
    {
        return histogram_.begin();
    }

    auto
    end() const
    {
        return histogram_.end();
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
    std::map<double, uint64_t> histogram_;
    uint64_t total_ = 0;
};
