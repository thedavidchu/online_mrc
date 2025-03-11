#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <unordered_map>

#include "cache/base_cache.hpp"
#include "cpp_cache/cache_metadata.hpp"
#include "cpp_cache/cache_statistics.hpp"
#include "ttl/ttl.hpp"
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
                // Re-insert visited node.
                map_.at(victim_key).unvisit();
                evictor_.push_front(victim_key);
                continue;
            } else {
                // Permanently remove unvisited node.
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
            std::cout << "validate(name=" << name << ",verbose=" << verbose
                      << ")" << std::endl;
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
    access_item(CacheAccess const &access)
    {
        assert(map_.size() == evictor_.size());
        assert(map_.size() <= capacity_);
        if (capacity_ == 0) {
            statistics_.deprecated_miss();
            return 0;
        }

        // TODO Change this to enable TTLs.
        std::uint64_t expiration_time_ms =
            get_expiration_time(access.timestamp_ms, FOREVER);
        if (map_.count(access.key)) {
            hit(access.timestamp_ms, access.key, expiration_time_ms);
            statistics_.deprecated_hit();
        } else {
            miss(access.timestamp_ms, access.key, expiration_time_ms);
            statistics_.deprecated_miss();
        }
        assert(map_.size() <= capacity_);
        return 0;
    }

protected:
    std::deque<std::uint64_t> evictor_;

public:
    static constexpr char name[] = "ClockCache";
};
