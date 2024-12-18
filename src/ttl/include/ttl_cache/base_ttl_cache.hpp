#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cache_statistics/cache_statistics.hpp"
#include "math/saturation_arithmetic.h"

static inline std::uint64_t
get_expiration_time(std::uint64_t const current_time_ms,
                    std::uint64_t const ttl_s)
{
    return saturation_add(current_time_ms, saturation_multiply(1000, ttl_s));
}

struct TTLMetadata {
    /// @note   We don't consider the first access in the frequency
    ///         counter. There's no real reason, I just think it's nice
    ///         to start at 0 rather than 1.
    std::size_t frequency_ = 0;
    std::uint64_t insertion_time_ms_ = 0;
    std::uint64_t last_access_time_ms_ = 0;
    /// @note   I decided to store the expiration time rather than the
    ///         TTL for convenience. The TTL can be calculated by
    ///         subtracting the (last time the expiration time was set)
    ///         from the (expiration time).
    std::uint64_t expiration_time_ms_ = 0;

    TTLMetadata(std::uint64_t const insertion_time_ms,
                std::uint64_t const expiration_time_ms)
        : insertion_time_ms_(insertion_time_ms),
          last_access_time_ms_(insertion_time_ms),
          expiration_time_ms_(expiration_time_ms)
    {
    }

    template <class Stream>
    void
    to_stream(Stream &s, bool const newline = false) const
    {
        s << "TTLMetadata(frequency=" << frequency_ << ",";
        s << "insertion_time[ms]=" << insertion_time_ms_ << ",";
        s << "last_access_time[ms]=" << last_access_time_ms_ << ",";
        s << "expiration_time[ms]=" << expiration_time_ms_ << ")";
        if (newline) {
            s << std::endl;
        }
    }

    void
    visit(std::uint64_t const access_time_ms,
          std::optional<std::uint64_t> const new_expiration_time_ms)
    {
        ++frequency_;
        last_access_time_ms_ = access_time_ms;
        if (new_expiration_time_ms) {
            expiration_time_ms_ = new_expiration_time_ms.value();
        }
    }
};

class BaseTTLCache {
public:
    BaseTTLCache(std::size_t const capacity)
        : capacity_(capacity)
    {
    }

    std::uint64_t
    size() const
    {
        return map_.size();
    }

    /// @brief  Get keys in current eviction order (from soonest to
    ///         furthest eviction time).
    std::vector<std::uint64_t>
    get_keys() const
    {
        std::vector<std::uint64_t> keys;
        keys.reserve(capacity_);
        for (auto [exp_tm, key] : expiration_queue_) {
            keys.push_back(key);
        }
        return keys;
    }

    /// @brief  Get {expiration time, key} pair of the soonest expiring
    ///         data.
    std::optional<std::pair<std::uint64_t, std::uint64_t>>
    get_soonest_expiring()
    {
        auto begin = expiration_queue_.begin();
        if (begin == expiration_queue_.end()) {
            return {};
        }
        return {{begin->first, begin->second}};
    }

    std::optional<std::uint64_t>
    evict_soonest_expiring()
    {
        auto const it = expiration_queue_.begin();
        if (it == expiration_queue_.end()) {
            return std::nullopt;
        }
        std::uint64_t victim_key = it->second;
        expiration_queue_.erase(it);
        std::size_t i = map_.erase(victim_key);
        assert(i == 1);
        return victim_key;
    }

    /// @brief  Change the expiration time of an object.
    bool
    update_expiration_time(std::uint64_t const old_expiration_time_ms,
                           std::uint64_t const key,
                           std::uint64_t const new_expiration_time_ms)
    {
        auto [begin, end] =
            expiration_queue_.equal_range(old_expiration_time_ms);
        for (auto x = begin; x != end; ++x) {
            if (x->second == key) {
                auto nh = expiration_queue_.extract(x);
                nh.key() = new_expiration_time_ms;
                expiration_queue_.insert(std::move(nh));
                return true;
            }
        }
        return true;
    }

    template <class Stream>
    void
    to_stream(Stream &s) const
    {
        s << name << "(capacity=" << capacity_ << ",size=" << size() << ")"
          << std::endl;
        s << "> Key-Metadata Map:" << std::endl;
        for (auto [k, metadata] : map_) {
            // This is inefficient, but looks easier on the eyes.
            std::stringstream ss;
            metadata.to_stream(ss);
            s << ">> key: " << k << ", metadata: " << ss.str() << std::endl;
        }
        s << "> Expiration Queue:" << std::endl;
        for (auto [exp_tm, k] : expiration_queue_) {
            s << ">> expiration time[ms]: " << exp_tm << ", key: " << k
              << std::endl;
        }
    }

    bool
    validate(int const verbose = 0) const
    {
        if (verbose) {
            std::cout << "validate(verbose=" << verbose << ")" << std::endl;
        }
        assert(map_.size() == expiration_queue_.size());
        assert(size() <= capacity_);
        if (verbose) {
            std::cout << "> size: " << size() << std::endl;
        }
        if (verbose >= 2) {
            to_stream(std::cout);
        }
        for (auto [k, metadata] : map_) {
            if (verbose >= 2) {
                std::stringstream ss;
                metadata.to_stream(ss);
                std::cout << "> Validating: key=" << k
                          << ", metadata=" << ss.str() << std::endl;
            }
            assert(expiration_queue_.count(metadata.expiration_time_ms_));
            auto obj = expiration_queue_.find(metadata.expiration_time_ms_);
            assert(obj != expiration_queue_.end());
            assert(obj->first == metadata.expiration_time_ms_);
            assert(obj->second == k);
        }
        return true;
    }

    int
    access_item(std::uint64_t const timestamp_ms,
                std::uint64_t const key,
                std::uint64_t const ttl_s);

protected:
    std::uint64_t const ttl_s_ = 1 << 30;
    std::size_t const capacity_;

    /// @brief  Map the keys to the metadata.
    std::unordered_map<std::uint64_t, TTLMetadata> map_;
    std::multimap<std::uint64_t, std::uint64_t> expiration_queue_;

public:
    static constexpr char name[] = "BaseTTLCache";
    CacheStatistics statistics_;
};
