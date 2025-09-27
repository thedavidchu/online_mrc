#pragma once

#include "accurate/accurate.hpp"
#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_metadata.hpp"
#include "cpp_lib/cache_statistics.hpp"
#include "cpp_lib/duration.hpp"
#include "cpp_lib/enumerate.hpp"
#include "cpp_lib/memory_size.hpp"
#include "cpp_lib/util.hpp"
#include "lib/eviction_cause.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <glib.h>
#include <map>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

constexpr static bool DEBUG = false;

class MemcachedSlabClass {
public:
    /// @param min_size_bytes, max_size_bytes
    ///     Minimum and maximum size of this slab class, inclusive (i.e.
    ///     these ends are valid sizes in this cache).
    MemcachedSlabClass(uint64_t const id,
                       uint64_t const min_size_bytes,
                       uint64_t const max_size_bytes)
        : id_(id),
          min_size_bytes_(min_size_bytes),
          max_size_bytes_(max_size_bytes)
    {
    }

    void
    validate_no_expired(
        double const current_time_ms,
        std::unordered_map<uint64_t, CacheMetadata> const &map) const
    {
        if (!DEBUG) {
            return;
        }
        g_assert_cmpuint(keys_.size(), ==, ttl_queue_.size());
        if (ttl_queue_.empty()) {
            return;
        }
        g_assert_cmpfloat(ttl_queue_.begin()->first, >=, current_time_ms);

        for (auto [exp_tm, key] : ttl_queue_) {
            g_assert_cmpfloat(map.at(key).expiration_time_ms_, ==, exp_tm);
        }
    }

    uint64_t
    id() const
    {
        return id_;
    }

    /// @brief  Get the next time [ms] when the expiry scan should be run
    ///         to remove stale keys.
    uint64_t
    next_expiry_scan(CacheAccess const &access)
    {
        static constexpr uint64_t MAX_MAINTCRAWL_WAIT_S = 60 * 60;
        std::vector<uint64_t> histo(60);
        uint64_t no_exp = 0;
        assert(access.timestamp_ms == next_crawl_time_ms_);

        // Taken from
        // https://github.com/memcached/memcached/blob/master/crawler.c.
        // Search for 'histo' for the correct code.
        for (auto [exp_tm_ms, key] : ttl_queue_) {
            if (std::isinf(exp_tm_ms)) {
                no_exp += 1;
            } else if (exp_tm_ms - access.timestamp_ms > 3599 * 1000) {
                // That's nice... but we don't care.
            } else {
                uint64_t ttl_remain_ms = exp_tm_ms - access.timestamp_ms;
                // A bucket represents a minute.
                uint64_t bucket = ttl_remain_ms / (60 * 1000);
                assert(bucket < 60);
                histo[bucket] += 1;
            }
        }

        // Taken from
        // https://github.com/memcached/memcached/blob/master/items.c.
        // Search for 'histo' for the correct code.
        /* Should we crawl again? */
        uint64_t possible_reclaims = ttl_queue_.size() - no_exp;
        uint64_t available_reclaims = 0;
        /* Need to think we can free at least 1% of the items before
         * crawling. */
        /* FIXME: Configurable? */
        uint64_t low_watermark = (possible_reclaims / 100) + 1;
        /* Don't bother if the payoff is too low. */
        for (uint64_t x = 0; x < 60; x++) {
            available_reclaims += histo[x];
            if (available_reclaims > low_watermark) {
                if (next_crawl_wait_s_ < (x * 60)) {
                    next_crawl_wait_s_ += 60;
                    nr_scan_increases_ += 1;
                } else if (next_crawl_wait_s_ >= 60) {
                    next_crawl_wait_s_ -= 60;
                    nr_scan_decreases_ += 1;
                }
                break;
            }
        }
        if (available_reclaims == 0) {
            next_crawl_wait_s_ += 60;
            nr_scan_increases_ += 1;
        }
        if (next_crawl_wait_s_ > MAX_MAINTCRAWL_WAIT_S) {
            next_crawl_wait_s_ = MAX_MAINTCRAWL_WAIT_S;
        } else if (next_crawl_wait_s_ == 0) {
            next_crawl_wait_s_ = 60;
        }
        nr_scans_ += 1;
        scan_time_intervals_ms_ += next_crawl_wait_s_ * Duration::SECOND;
        last_crawl_time_ms_ = access.timestamp_ms;
        next_crawl_time_ms_ =
            access.timestamp_ms + next_crawl_wait_s_ * Duration::SECOND;
        max_crawl_wait_ms_ =
            std::max(max_crawl_wait_ms_, next_crawl_wait_s_ * Duration::SECOND);
        return next_crawl_time_ms_;
    }

    /// @brief  Remove stale keys and return a list of them.
    std::vector<uint64_t>
    get_expired(CacheAccess const &access)
    {
        std::vector<uint64_t> victims;
        for (auto [exp_tm, key] : ttl_queue_) {
            assert(!std::isnan(exp_tm));
            if (exp_tm >= access.timestamp_ms) {
                break;
            }
            assert(keys_.contains(key));
            victims.push_back(key);
        }
        nr_discards_ += victims.size();
        nr_searches_ += ttl_queue_.size();
        return victims;
    }

    void
    insert(CacheAccess const &access)
    {
        assert(!keys_.contains(access.key));
        g_assert_cmpuint(min_size_bytes_, <=, access.size_bytes());
        g_assert_cmpuint(access.size_bytes(), <=, max_size_bytes_);
        keys_.insert(access.key);
        ttl_queue_.emplace(access.expiration_time_ms(), access.key);
    }
    void
    update(CacheAccess const &access)
    {
        g_assert_cmpuint(min_size_bytes_, <=, access.size_bytes());
        g_assert_cmpuint(access.size_bytes(), <=, max_size_bytes_);
        assert(keys_.contains(access.key));
    }
    void
    remove(uint64_t const key,
           EvictionCause const cause,
           CacheMetadata const &metadata,
           CacheAccess const &access)
    {
        if (cause == EvictionCause::AccessExpired) {
            g_assert_cmpfloat(metadata.expiration_time_ms_,
                              >=,
                              last_crawl_time_ms_);
            nr_lazy_discards_ += 1;
            total_lazy_expiry_ms_ +=
                access.timestamp_ms - metadata.expiration_time_ms_;
        }
        if (keys_.contains(key)) {
            keys_.erase(key);
            bool r = remove_multimap_kv(ttl_queue_,
                                        metadata.expiration_time_ms_,
                                        key);
            assert(r);
        } else {
            assert(0 && "key DNE");
        }
    }
    std::pair<uint64_t, uint64_t>
    range() const
    {
        return {min_size_bytes_, max_size_bytes_};
    }
    uint64_t
    count_keys() const
    {
        return keys_.size();
    }

    std::string
    stats() const
    {
        return map2str(std::vector<std::pair<std::string, std::string>>{
            {"Scan Increases [#]", val2str(nr_scan_increases_)},
            {"Scan Decreases [#]", val2str(nr_scan_decreases_)},
            {"Searched Objects [#]", val2str(nr_searches_)},
            {"Discarded Objects [#]", val2str(nr_discards_)},
            {"Scans [#]", val2str(nr_scans_)},
            {"Total Scan Time Intervals [min]",
             val2str(scan_time_intervals_ms_ / 60000)},
            {"Mean Scan Interval [min]",
             val2str((double)scan_time_intervals_ms_ / 60000 / nr_scans_)},
            // TODO ADD STATS
            {"Lazy Discarded Objects [#]", val2str(nr_lazy_discards_)},
            {"Mean Lazy Discard Overstay [min]",
             val2str((double)total_lazy_expiry_ms_ / 60000 /
                     nr_lazy_discards_)},
            {"Last Scan Interval [min]", val2str(next_crawl_wait_s_ / 60)},
            {"Max Scan Interval [min]", val2str(max_crawl_wait_ms_ / 60000)},
        });
    }

    uint64_t const id_;
    uint64_t min_size_bytes_;
    uint64_t max_size_bytes_;
    // This is in seconds for now, because that's what Memcached uses.
    // The initial value is 60 seconds.
    uint64_t next_crawl_wait_s_ = 60;
    uint64_t last_crawl_time_ms_ = 0;
    uint64_t next_crawl_time_ms_ = 0;
    std::unordered_set<uint64_t> keys_;
    // Maps expiration time to keys.
    std::multimap<double, uint64_t> ttl_queue_;

    // Statistics
    uint64_t max_crawl_wait_ms_ = 0;
    uint64_t nr_scan_increases_ = 0;
    uint64_t nr_scan_decreases_ = 0;
    uint64_t nr_discards_ = 0;
    uint64_t nr_searches_ = 0;
    uint64_t nr_scans_ = 0;
    uint64_t scan_time_intervals_ms_ = 0;
    uint64_t nr_lazy_discards_ = 0;
    uint64_t total_lazy_expiry_ms_ = 0;
};

struct MemcachedExpiryStatistics {
    /// @brief  Stringify the expiry statistics.
    /// @note   We compress it into an array because it has a large
    //          overhead otherwise.
    std::string
    json() const
    {
        return vec2str(std::vector<uint64_t>{id,
                                             time_ms,
                                             nr_objects,
                                             nr_expired,
                                             next_time_ms});
    }

    uint64_t id;
    uint64_t time_ms;
    uint64_t nr_objects;
    uint64_t nr_expired;
    uint64_t next_time_ms;
};

class MemcachedTTL : public Accurate {
private:
    void
    validate()
    {
        uint64_t nr_obj = 0, nr_ttl_obj = 0;
        for (auto &cls : slab_classes_) {
            nr_obj += cls.keys_.size();
            nr_ttl_obj += cls.ttl_queue_.size();
        }
        g_assert_cmpuint(nr_obj, ==, map_.size());
        g_assert_cmpuint(nr_ttl_obj, ==, map_.size());
    }

    MemcachedSlabClass *
    get_slab_class(uint64_t const size_bytes)
    {
        using namespace MemorySize;
        switch (size_bytes) {
        case 0 ... 1 * KiB - 1:
            return &slab_classes_[0];
        case 1 * KiB... 2 * KiB - 1:
            return &slab_classes_[0];
        case 2 * KiB... 4 * KiB - 1:
            return &slab_classes_[1];
        case 4 * KiB... 8 * KiB - 1:
            return &slab_classes_[2];
        case 8 * KiB... 16 * KiB - 1:
            return &slab_classes_[3];
        case 16 * KiB... 32 * KiB - 1:
            return &slab_classes_[4];
        case 32 * KiB... 64 * KiB - 1:
            return &slab_classes_[5];
        case 64 * KiB... 128 * KiB - 1:
            return &slab_classes_[6];
        case 128 * KiB... 256 * KiB - 1:
            return &slab_classes_[7];
        case 256 * KiB... 512 * KiB - 1:
            return &slab_classes_[8];
        case 512 * KiB... 1024 * KiB - 1:
            return &slab_classes_[9];
        case 1 * MiB... 2 * MiB - 1:
            return &slab_classes_[10];
        case 2 * MiB... 4 * MiB - 1:
            return &slab_classes_[11];
        case 4 * MiB... 8 * MiB - 1:
            return &slab_classes_[12];
        case 8 * MiB... 16 * MiB - 1:
            return &slab_classes_[13];
        case 16 * MiB... 32 * MiB - 1:
            return &slab_classes_[14];
        case 32 * MiB... 64 * MiB - 1:
            return &slab_classes_[15];
        case 64 * MiB... 128 * MiB - 1:
            return &slab_classes_[16];
        case 128 * MiB... 256 * MiB - 1:
            return &slab_classes_[17];
        case 256 * MiB... 512 * MiB - 1:
            return &slab_classes_[18];
        case 512 * MiB... 1024 * MiB - 1:
            return &slab_classes_[19];
        default:
            assert(0 && "too big");
            return nullptr;
        }
    }

    void
    insert(CacheAccess const &access) override final
    {
        assert(!map_.contains(access.key));
        statistics_.insert(access.size_bytes());
        CacheMetadata const m{access};
        map_.emplace(access.key, m);
        size_bytes_ += access.size_bytes();
        get_slab_class(access.size_bytes())->insert(access);
    }

    void
    update(CacheAccess const &access, CacheMetadata &metadata) override final
    {
        assert(map_.contains(access.key));
        auto pre_cls = get_slab_class(metadata.size_);
        size_bytes_ += access.size_bytes() - metadata.size_;
        statistics_.update(metadata.size_, access.size_bytes());
        metadata.visit_without_ttl_refresh(access);
        auto post_cls = get_slab_class(metadata.size_);
        if (pre_cls == post_cls) {
            pre_cls->update(access);
        } else {
            // Move the object to a different slab class.
            pre_cls->remove(access.key, EvictionCause::Other, metadata, access);
            post_cls->insert(access);
        }
    }

    /// @note   We assume the object has already been removed from the
    ///         slab class.
    void
    remove(uint64_t const victim_key,
           EvictionCause const cause,
           CacheAccess const &access) override final
    {
        assert(map_.contains(victim_key));
        CacheMetadata &m = map_.at(victim_key);
        uint64_t sz_bytes = m.size_;

        // Update metadata tracking
        switch (cause) {
        case EvictionCause::ProactiveTTL:
            statistics_.ttl_expire(m.size_);
            break;
        case EvictionCause::AccessExpired:
            statistics_.lazy_expire(m.size_, m.ttl_ms(access.timestamp_ms));
            break;
        default:
            assert(0 && "impossible");
        }

        size_bytes_ -= sz_bytes;
        auto cls = get_slab_class(sz_bytes);
        cls->remove(victim_key, cause, m, access);
        map_.erase(victim_key);
    }

    void
    remove_expired(CacheAccess const &access) override final
    {
        std::vector<std::pair<uint64_t, uint64_t>> new_work;
        auto range = schedule_.equal_range(access.timestamp_ms);
        for (auto it = range.first; it != range.second; ++it) {
            expiry_cycles_ += 1;
            auto id = it->second;
            auto &cls = slab_classes_[id];
            // Memcached scans the entire slab class for expired objects.
            // We must do this before removing the keys.
            expiration_work_ += cls.count_keys();
            auto victims = cls.get_expired(access);
            for (auto v : victims) {
                remove(v, EvictionCause::ProactiveTTL, access);
            }
            // I am getting a lot of suspicious lazy expiries, so I want to
            // ensure that no expired objects remain in the TTL queue.
            cls.validate_no_expired(access.timestamp_ms, map_);
            nr_expirations_ += victims.size();
            auto next_scan_ms = cls.next_expiry_scan(access);
            new_work.emplace_back(next_scan_ms, id);
        }
        schedule_.erase(access.timestamp_ms);
        // Add the new work after we have finished erasing from the schedule
        // to make absolute sure we don't erase any new work. In C++23, this
        // would be insert_range().
        for (auto &[next_time, id] : new_work) {
            schedule_.emplace(next_time, id);
        }
    }

public:
    /// @param  capacity: size_t - The capacity of the cache in bytes.
    MemcachedTTL(uint64_t const capacity_bytes,
                 double const shards_sampling_ratio)
        : Accurate{capacity_bytes, shards_sampling_ratio}
    {
        using namespace MemorySize;
        // TODO Modern Memcached doesn't use power-of-2 slab classes.
        //      You'll have to figure out how to fix this.
        // Memcached slab sizes range from 1 KiB to 1 GiB. Source:
        // https://cloud.google.com/memorystore/docs/memcached/best-practices
        for (uint64_t i = 0; i < 20; ++i) {
            // The first slab-class handles objects between 0 B to 2 KiB.
            slab_classes_.push_back(
                MemcachedSlabClass{i,
                                   i == 0 ? 0 : (1 << i) * KiB,
                                   (1 << (i + 1)) * KiB - 1});
            schedule_.emplace(0, i);
        }
    }

    std::string
    json(std::unordered_map<std::string, std::string> extras = {})
    {
        std::function<std::string(MemcachedExpiryStatistics const &)> x =
            [](MemcachedExpiryStatistics const &val) -> std::string {
            return val.json();
        };
        std::vector<std::pair<std::string, std::string>> cls_stats;
        for (auto [id, x] : enumerate(slab_classes_)) {
            cls_stats.emplace_back(val2str(id), x.stats());
        }

        auto r = p_json_vector(extras);
        // This uses a lot of storage.
        if (false) {
            r.emplace_back("Memcached Expiry Statistics", vec2str(stats_, x));
        }
        r.emplace_back("Memcached Class Statistics", map2str(cls_stats));
        return map2str(r);
    }

private:
    // The schedule to check for expired slab classes.
    std::multimap<uint64_t, uint64_t> schedule_;
    std::vector<MemcachedSlabClass> slab_classes_;
    std::vector<MemcachedExpiryStatistics> stats_;
};
