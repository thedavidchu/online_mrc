#pragma once

#include "accurate/accurate.hpp"
#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_metadata.hpp"
#include "cpp_lib/cache_statistics.hpp"
#include "cpp_lib/duration.hpp"
#include "cpp_lib/memory_size.hpp"
#include "cpp_lib/util.hpp"
#include "lib/eviction_cause.hpp"
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <glib.h>
#include <map>
#include <stdlib.h>
#include <sys/types.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

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
                } else if (next_crawl_wait_s_ >= 60) {
                    next_crawl_wait_s_ -= 60;
                }
                break;
            }
        }
        if (available_reclaims == 0) {
            next_crawl_wait_s_ += 60;
        }
        if (next_crawl_wait_s_ > MAX_MAINTCRAWL_WAIT_S) {
            next_crawl_wait_s_ = MAX_MAINTCRAWL_WAIT_S;
        }
        return access.timestamp_ms + next_crawl_wait_s_ * Duration::SECOND;
    }

    /// @brief  Remove stale keys and return a list of them.
    std::vector<uint64_t>
    get_expired(CacheAccess const &access)
    {
        std::vector<uint64_t> victims;
        for (auto [exp_tm, key] : ttl_queue_) {
            if (exp_tm >= access.timestamp_ms) {
                break;
            }
            assert(keys_.contains(key));
            victims.push_back(key);
        }
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
    remove(uint64_t const key, CacheMetadata const &metadata)
    {
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

    uint64_t const id_;
    uint64_t min_size_bytes_;
    uint64_t max_size_bytes_;
    // This is in seconds for now, because that's what Memcached uses.
    // The initial value is 60 seconds.
    uint64_t next_crawl_wait_s_ = 60;
    std::unordered_set<uint64_t> keys_;
    // Maps expiration time to keys.
    std::multimap<double, uint64_t> ttl_queue_;
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
            pre_cls->remove(access.key, metadata);
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
        cls->remove(victim_key, m);
        map_.erase(victim_key);
    }

    void
    remove_expired(CacheAccess const &access) override final
    {
        CacheAccess pseudo{access};
        for (; current_time_ms_ <= access.timestamp_ms;
             current_time_ms_ += 1 * Duration::SECOND) {
            pseudo.timestamp_ms = current_time_ms_;
            std::vector<std::pair<uint64_t, uint64_t>> new_work;
            auto range = schedule_.equal_range(current_time_ms_);
            for (auto it = range.first; it != range.second; ++it) {
                auto id = it->second;
                auto &cls = slab_classes_[id];
                // Memcached scans the entire slab class for expired objects.
                // We must do this before removing the keys.
                expiration_work_ += cls.count_keys();
                auto nr_keys = cls.count_keys();
                auto victims = cls.get_expired(pseudo);
                for (auto v : victims) {
                    remove(v, EvictionCause::ProactiveTTL, pseudo);
                }
                nr_expirations_ += victims.size();
                auto next_scan_ms = cls.next_expiry_scan(pseudo);
                stats_.emplace_back(id,
                                    current_time_ms_,
                                    nr_keys,
                                    victims.size(),
                                    next_scan_ms);
                new_work.emplace_back(next_scan_ms, id);
            }
            schedule_.erase(schedule_.begin(),
                            schedule_.upper_bound(current_time_ms_));
            // Add the new work after we have finished erasing from the schedule
            // to make absolute sure we don't erase any new work. In C++23, this
            // would be insert_range().
            for (auto &[next_time, id] : new_work) {
                schedule_.emplace(next_time, id);
            }
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

    void
    access(CacheAccess const &access)
    {
        validate();
        assert(size_bytes_ == statistics_.size_);
        statistics_.time(access.timestamp_ms);
        assert(size_bytes_ == statistics_.size_);
        remove_accessed_if_expired(access);
        remove_expired(access);
        if (map_.contains(access.key)) {
            update(access, map_.at(access.key));
        } else {
            insert(access);
        }
    }

    std::string
    json(std::unordered_map<std::string, std::string> extras = {})
    {
        std::function<std::string(MemcachedExpiryStatistics const &)> x =
            [](MemcachedExpiryStatistics const &val) -> std::string {
            return val.json();
        };
        return map2str(std::vector<std::pair<std::string, std::string>>{
            {"Capacity [B]", val2str(format_memory_size(capacity_bytes_))},
            {"Statistics", statistics_.json()},
            {"Extras", map2str(extras)},
            {"Expiration Work [#]", std::to_string(expiration_work_)},
            {"Expirations [#]", std::to_string(nr_expirations_)},
            {"Lazy Expirations [#]", std::to_string(nr_lazy_expirations_)},
            // {"Memcached Expiry Statistics", vec2str(stats_, x)},
        });
    }

private:
    // The schedule to check for expired slab classes.
    std::multimap<uint64_t, uint64_t> schedule_;
    std::vector<MemcachedSlabClass> slab_classes_;
    std::vector<MemcachedExpiryStatistics> stats_;

    uint64_t current_time_ms_ = 0;
};
