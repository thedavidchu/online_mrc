#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <unordered_map>

#include "cache/base_cache.hpp"
#include "cache_metadata/cache_metadata.hpp"
#include "cache_statistics/cache_statistics.hpp"
#include "ttl_cache/base_ttl_cache.hpp"
#include "unused/mark_unused.h"

/// @note   I implemented this as FIFO with reinsertion, which is slower
///         but easier to get right.
class ClockCache : public BaseCache {
private:
    void
    hit(std::uint64_t const access_time_ms,
        std::uint64_t const key,
        std::uint64_t const expiration_time_ms)
    {
        map_.at(key).visit(access_time_ms, expiration_time_ms);
    }

    void
    miss(std::uint64_t const access_time_ms,
         std::uint64_t const key,
         std::uint64_t const expiration_time_ms)
    {
        assert(evictor_.size() <= capacity_);
        while (evictor_.size() >= capacity_) {
            std::uint64_t const victim_key = evictor_.back();
            evictor_.pop_back();
            if (map_.at(victim_key).visited) {
                map_.at(victim_key).unvisit();
                evictor_.push_front(victim_key);
                continue;
            } else {
                map_.erase(victim_key);
                break;
            }
        }
        evictor_.push_front(key);
        map_.emplace(key, CacheMetadata(access_time_ms, expiration_time_ms));
    }

public:
    ClockCache(std::size_t capacity)
        : BaseCache(capacity)
    {
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
        s << "> Evictor" << std::endl;
        for (auto k : evictor_) {
            s << ">> key: " << k << std::endl;
        }
    }

    bool
    validate(int const verbose = 0) const
    {
        if (verbose) {
            std::cout << "validate(verbose=" << verbose << ")" << std::endl;
        }
        assert(map_.size() == evictor_.size());
        assert(size() <= capacity_);
        if (verbose) {
            std::cout << "> size: " << size() << std::endl;
        }
        if (verbose >= 2) {
            to_stream(std::cout);
        }

        for (auto k : evictor_) {
            if (verbose >= 2) {
                std::cout << "> Validating: key=" << k << std::endl;
            }
            assert(map_.count(k));
        }
        return true;
    }

    int
    access_item(std::uint64_t const access_time_ms,
                std::uint64_t const key,
                std::uint64_t const ttl_s)
    {
        UNUSED(ttl_s);
        assert(map_.size() == evictor_.size());
        assert(map_.size() <= capacity_);
        if (capacity_ == 0) {
            statistics_.miss();
            return 0;
        }

        std::uint64_t expiration_time_ms =
            get_expiration_time(access_time_ms, FOREVER);
        if (map_.count(key)) {
            hit(access_time_ms, key, expiration_time_ms);
            statistics_.hit();
        } else {
            miss(access_time_ms, key, expiration_time_ms);
            statistics_.miss();
        }
        assert(map_.size() <= capacity_);
        return 0;
    }

protected:
    // This is to prevent the object from expiring.
    std::size_t const FOREVER = std::numeric_limits<std::size_t>::max();
    std::deque<std::uint64_t> evictor_;

public:
    static constexpr char name[] = "ClockCache";
};
