#pragma once

#include "logger/logger.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using uint64_t = std::uint64_t;

/// @todo   Could this be made more accurate if we track the exact
///         values rather than the number of leading zeroes?
///         | Binary | NLZ | Soft-NLZ |
///         ---------|-----|----------|
///         |>0b1xxx<| >0< |>0.x<     |
///         | 0b1111 |  0  | 0.0      |
///         | 0b1110 |  0  | 0.1      |
///         | 0b1101 |  0  | 0.2      |
///         | 0b1100 |  0  | 0.3      |
///         | 0b1011 |  0  | 0.4      |
///         | 0b1010 |  0  | 0.5      |
///         | 0b1001 |  0  | 0.7      |
///         | 0b1000 |  0  | 0.8      |
///         |>0b01xx<| >1< |>1.x<     |
///         | 0b0111 |  1  | 1.0      |
///         | 0b0110 |  1  | 1.2      |
///         | 0b0101 |  1  | 1.4      |
///         | 0b0100 |  1  | 1.7      |
///         |>0b001x<| >2< |>2.x<     |
///         | 0b0011 |  2  | 2.0      |
///         | 0b0010 |  2  | 2.4      |
///         |>0b0001<| >3< |>3.0<     |
///         |>0b0000<| >4< |>4.0<     |
///         The formula is (nr_bits - log2(x + 1))
class HyperLogLog {
private:
    /// Source:
    /// https://en.wikipedia.org/wiki/HyperLogLog#Practical_considerations
    static double
    hll_alpha_m(uint64_t const m)
    {
        if (m == 16) {
            return 0.673;
        } else if (m == 32) {
            return 0.697;
        } else if (m == 64) {
            return 0.709;
        } else if (m >= 128) {
            return 0.7213 / (1 + 1.079 / m);
        } else {
            LOGGER_WARN(
                "unsupported HyperLogLog size of %zu, not using fudge factor!",
                m);
            return 1.0;
        }
    }

    double
    calculate_fresh_inv_Z() const
    {
        double r = 0.0;
        for (auto m : M_) {
            r += std::exp2(-m - 1);
        }
        return r;
    }

    uint64_t
    calculate_fresh_V() const
    {
        uint64_t r = 0;
        for (auto m : M_) {
            // I rely on C++ casting 'true' to 1 and 'false' to 0.
            r += !m;
        }
        return r;
    }

    double
    linear_counting() const;

public:
    HyperLogLog(uint64_t const nr_buckets);
    void
    add(uint64_t const hash);

    /// @brief  Get the cardinality estimate.
    uint64_t
    count() const;

    bool
    imerge(HyperLogLog const &hll)
    {
        if (hll.m_ != m_) {
            return false;
        }
        for (uint64_t i = 0; i < m_; ++i) {
            M_[i] = std::max(M_[i], hll.M_[i]);
        }
        V_ = calculate_fresh_V();
        inv_Z_ = calculate_fresh_inv_Z();
        return true;
    }

    /// @brief  Get the number of buckets.
    uint64_t
    size() const;

    std::string
    json() const;

private:
    // Correction factor, which we approximate, as per Wikipedia.
    double const alpha_m_;
    // Number of buckets in M_.
    uint64_t const m_;
    // Number of zeroes in M_ (i.e. count the entries in M_ where the
    // number of leading zeroes is zero). This is slightly different
    // than my earlier implementations, which checked whether the value
    // was changed, rather than that the value is no longer 0. Admittedly,
    // I applied sampling over top of it, so I would only adjust when it
    // did change the number of leading zeroes to a non-zero number.
    uint64_t V_;
    // Number of leading zeros.
    std::vector<uint8_t> M_;
    // Z = 1 / sum(2 ** -x for x in M), so inv_Z = sum(2 ** -x for x in M).
    double inv_Z_;
};
