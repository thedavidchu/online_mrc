#include "hyperloglog/hyperloglog.hpp"
#include "cpp_lib/util.hpp"
#include "math/count_leading_zeros.h"
#include <algorithm>
#include <cstdint>
#include <sstream>

double
HyperLogLog::linear_counting() const
{
    return m_ * std::log((double)m_ / V_);
}

HyperLogLog::HyperLogLog(uint64_t const nr_buckets)
    : alpha_m_(hll_alpha_m(nr_buckets)),
      m_(nr_buckets),
      V_(m_),
      inv_Z_((double)nr_buckets / 2)
{
    M_.resize(m_);
    std::fill(M_.begin(), M_.end(), 0);
}

void
HyperLogLog::add(uint64_t const hash)
{
    uint64_t const nlz_h = clz(hash);
    // NOTE This isn't as efficient as if we had defined the number
    //      of buckets to be a power of 2, but optimize later.
    auto &cur_nlz = M_[hash % m_];
    if (nlz_h > cur_nlz) {
        // Due to the inequality, we know that the incoming number
        // of leading zeroes is greater than 0, so we don't need to
        // check again.
        if (cur_nlz == 0) {
            assert(nlz_h > 0);
            V_ -= 1;
        }
        inv_Z_ -= std::exp2(-(double)cur_nlz - 1);
        inv_Z_ += std::exp2(-(double)nlz_h - 1);
        cur_nlz = nlz_h;
    }
}

uint64_t
HyperLogLog::count() const
{
    double const raw_E = alpha_m_ * m_ * m_ / inv_Z_;
    if (raw_E < 2.5 * m_ && V_ != 0) {
        return linear_counting();
    }
    return raw_E;
}

uint64_t
HyperLogLog::size() const
{
    return M_.size();
}

std::string
HyperLogLog::json() const
{
    std::stringstream ss;
    ss << "{";
    ss << "\".type\": \"HyperLogLog\",";
    ss << "\"\\alpha_m\": " << alpha_m_ << ",";
    ss << "\"m\": " << m_ << ",";
    ss << "\"V\": " << V_ << ",";
    ss << "\"M\": " << vec2str<uint8_t, int>(M_) << ",";
    ss << "\"inverted Z\": " << inv_Z_ << ",";
    ss << "}";
    return ss.str();
}
