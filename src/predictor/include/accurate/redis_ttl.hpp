/** Simulate the Redis cache's TTL policy.
 *
 *  @note   All experiments will run on cache sizes larger than the TTL WSS.
 *          This is to avoid having to implement the other eviction policies.
 */
#pragma once

#include "accurate/accurate.hpp"
#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_metadata.hpp"
#include "cpp_lib/cache_statistics.hpp"
#include "cpp_lib/duration.hpp"
#include "cpp_lib/util.hpp"
#include "lib/eviction_cause.hpp"
#include "logger/logger.h"
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <ostream>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class RedisSampler {
    enum Validity { INVALID = 0, TOMBSTONE = 1, VALID = 2 };

    void
    check(std::string const &file, int line) const
    {
        uint64_t size = 0;
        for (auto [key, validity] : table_) {
            if (validity == VALID) {
                ++size;
            }
        }
        if (size != size_) {
            std::cout << "error " + file + ":" + std::to_string(line) << " -- "
                      << "counted " << size << " != expected " << size_
                      << std::endl;
            assert(size == size_);
        }
    }
    /// @brief  Hash a key.
    /// @note   I assume the key is already the product of MurmurHash3,
    ///         so we don't need to rehash.
    static uint64_t
    hash(uint64_t const key)
    {
        return key;
    }
    /// @brief  Get home position of a key.
    static uint64_t
    home_position(std::vector<std::pair<uint64_t, Validity>> const &table,
                  uint64_t const key)
    {
        return hash(key) % table.size();
    }
    static bool
    is_empty(std::vector<std::pair<uint64_t, Validity>> const &table,
             uint64_t const position)
    {
        return table[position % table.size()].second != VALID;
    }
    static bool
    is_valid(std::vector<std::pair<uint64_t, Validity>> const &table,
             uint64_t const position)
    {
        return table[position % table.size()].second == VALID;
    }
    /// @brief  Return the next match or non-valid (i.e. INVALID or TOMBSTONE)
    ///         position or SIZE upon failure.
    static uint64_t
    next_match_or_empty(std::vector<std::pair<uint64_t, Validity>> const &table,
                        uint64_t const position,
                        uint64_t const key)
    {
        for (uint64_t i = 0; i < table.size(); ++i) {
            uint64_t p = (position + i) % table.size();
            if ((is_valid(table, p) && table[p].first == key) ||
                is_empty(table, p)) {
                return p;
            }
        }
        return table.size();
    }
    /// @brief  Return next valid position or SIZE upon failure.
    uint64_t
    next_valid(uint64_t const position, uint64_t const step = 1) const
    {
        // Without this first check, we would scan the whole cache looking
        // for an object that simply is not there.
        if (size_ == 0) {
            return table_.size();
        }
        for (uint64_t i = 0; i < table_.size(); ++i) {
            uint64_t p = (position + step * i) % table_.size();
            if (is_valid(table_, p)) {
                return p;
            }
        }
        assert(0 && "impossible!");
        return table_.size();
    }
    /// @brief  Insert a key into a non-growing table.
    static bool
    p_insert(std::vector<std::pair<uint64_t, Validity>> &table,
             uint64_t &size,
             uint64_t const key)
    {
        uint64_t home = home_position(table, key);
        uint64_t p = next_match_or_empty(table, home, key);
        if (p >= table.size()) {
            return false;
        }
        table[p] = {key, VALID};
        ++size;
        return true;
    }
    void
    grow(uint64_t new_capacity)
    {
        assert(new_capacity > capacity());
        std::vector<std::pair<uint64_t, Validity>> new_table(new_capacity);
        uint64_t new_size = 0;
        for (auto [key, state] : table_) {
            if (state == VALID) {
                p_insert(new_table, new_size, key);
            }
        }
        table_ = std::move(new_table);
        size_ = new_size;
    }

public:
    RedisSampler(uint64_t const initial_capacity = 1024,
                 unsigned const rseed = 0)
        : table_{initial_capacity},
          prng_{rseed}
    {
    }
    uint64_t
    size() const
    {
        return size_;
    }
    uint64_t
    capacity() const
    {
        return table_.size();
    }
    /// @brief  Insert a key.
    bool
    insert(uint64_t const key)
    {
        if (size() >= (double)2 / 3 * capacity()) {
            grow(2 * capacity());
        }
        return p_insert(table_, size_, key);
    }
    /// @brief  Remove a key.
    bool
    remove(uint64_t const key)
    {
        uint64_t home = home_position(table_, key);
        for (uint64_t i = 0; i < capacity(); ++i) {
            uint64_t p = (home + i) % capacity();
            auto [candidate_key, validity] = table_[p];
            if (validity == INVALID) {
                return false;
            } else if (is_valid(table_, p) && candidate_key == key) {
                table_[p] = {0, TOMBSTONE};
                --size_;
                return true;
            }
        }
        return false;
    }
    /// @brief  Pick a random key.
    /// @note   We do NOT remove this random key by default.
    std::optional<uint64_t>
    sample(bool const remove_key = false)
    {
        if (capacity() == 0 || size_ == 0) {
            return std::nullopt;
        }
        uint64_t random = prng_();
        // I choose to step by 999983 because it is coprime with the table size
        // (power-of-2). This means that I will visit all the objects at least
        // once before visiting any of them a second time. I take a giant step
        // size (rather than just 1) because then the next object I sample has
        // no relationship to the fact I resolve collisions through linear
        // probing. A number smaller than the table size may be affected, in
        // theory. Too big of a number risks overflow.
        uint64_t pos = next_valid(random % capacity(), /*step=*/999983);
        if (pos >= capacity()) {
            return std::nullopt;
        }
        auto [key, validity] = table_[pos];
        if (remove_key) {
            remove(key);
        }
        return key;
    }
    /// @brief  Get a list of the keys.
    std::vector<uint64_t>
    keys() const
    {
        std::vector<uint64_t> r;
        r.reserve(size_);
        for (uint64_t i = 0; i < capacity(); ++i) {
            if (is_valid(table_, i)) {
                r.push_back(table_[i].first);
            }
        }
        assert(r.size() == size_);
        return r;
    }
    std::string
    json() const
    {
        std::unordered_map<Validity, std::string> validity2str{
            {INVALID, "INVALID"},
            {TOMBSTONE, "TOMBSTONE"},
            {VALID, "VALID"}};
        std::function<std::string(std::pair<uint64_t, Validity> const &)> f =
            [&validity2str](
                std::pair<uint64_t, Validity> const &val) -> std::string {
            return "{" + std::to_string(val.first) + ", " +
                   validity2str.at(val.second) + "}";
        };
        return "{\".size\": " + std::to_string(size()) +
               ", \".capacity\": " + std::to_string(capacity()) +
               ", \".table\": " + vec2str(table_, f) + "}";
    }

private:
    uint64_t size_ = 0;
    std::vector<std::pair<uint64_t, Validity>> table_;
    std::mt19937_64 prng_;
};

/// @todo   Look at
///         https://github.com/antirez/redis/blob/27dd3b71ceb90f639b74253298ab1174e9b08613/src/expire.c#L197
///         to get latest strategy (with 'effort' parameter). They also do
///         something to tune the amount of CPU usage.
class RedisTTL : public Accurate {
private:
    void
    ok() const
    {
        assert(0);
        auto keys = redis_sampler_.keys();
        for (auto k : keys) {
            assert(map_.contains(k));
        }
    }
    void
    remove(uint64_t const victim_key,
           EvictionCause const cause,
           CacheAccess const &access) override final
    {
        assert(map_.contains(victim_key));
        CacheMetadata &m = map_.at(victim_key);
        uint64_t sz_bytes = m.size_;
        double exp_tm = m.expiration_time_ms_;

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
        map_.erase(victim_key);
        redis_sampler_.remove(victim_key);
        remove_multimap_kv(ttl_queue_, exp_tm, victim_key);
    }

    void
    insert(CacheAccess const &access) override final
    {
        statistics_.insert(access.size_bytes());
        map_.emplace(access.key, CacheMetadata{access});
        size_bytes_ += access.size_bytes();
        redis_sampler_.insert(access.key);
    }

    void
    update(CacheAccess const &access, CacheMetadata &metadata) override final
    {
        size_bytes_ += access.size_bytes() - metadata.size_;
        statistics_.update(metadata.size_, access.size_bytes());
        metadata.visit_without_ttl_refresh(access);
    }

    /// @brief  Soon-to-expire objects are stored in a tree from which we evict.
    void
    remove_expired_from_tree(CacheAccess const &access)
    {
        std::vector<uint64_t> victims;
        for (auto [exp_tm, key] : ttl_queue_) {
            if (exp_tm >= access.timestamp_ms) {
                break;
            }
            victims.push_back(key);
        }
        // One cannot erase elements from a multimap while also iterating!
        for (auto victim : victims) {
            remove(victim, EvictionCause::ProactiveTTL, access);
        }
        expiration_work_ += victims.size();
        nr_expirations_ += victims.size();
    }

    /// @param  start: bool - whether
    /// @return Whether another round of sampling should occur.
    bool
    remove_expired_via_sampling(CacheAccess const &access)
    {
        // We will escape from the search unless we explicitly find
        // enough expired objects.
        double ratio_expired = 0.0;
        expiry_cycles_ += 1;
        for (uint64_t i = 0; i < NUMBER_SAMPLES; ++i) {
            // Mark expired
            std::optional<uint64_t> key = redis_sampler_.sample();
            // Check that there are enough objects to sample.
            if (!key) {
                break;
            }
            expiration_work_ += 1;
            assert(map_.contains(key.value()));
            auto m = map_.at(key.value());
            if (m.ttl_ms(access.timestamp_ms) < 0.0) {
                ratio_expired += (double)1 / NUMBER_SAMPLES;
                remove(key.value(), EvictionCause::ProactiveTTL, access);
                nr_expirations_ += 1;
            } else if (m.ttl_ms(access.timestamp_ms) <
                           SOON_EXPIRING_THRESHOLD_MS &&
                       find_multimap_kv(ttl_queue_,
                                        m.expiration_time_ms_,
                                        key.value()) != ttl_queue_.end()) {
                ttl_queue_.emplace(m.expiration_time_ms_, key.value());
            }
        }
        return ratio_expired > ACCEPTABLE_STALE_RATIO;
    }

    void
    remove_expired(CacheAccess const &access) override final
    {
        static bool run_once = false;
        remove_expired_from_tree(access);
        // This is for SHARDS sampling that is some multiple of 10.
        // e.g. {10, 20, 30, 100, 1000, ...}.
        if (shards_.scale >= 10) {
            if (!run_once && shards_.scale % 10 != 0) {
                LOGGER_WARN("scale %zu does not divide nicely by 10",
                            shards_.scale);
                run_once = true;
            }
            // SHARDS reduces the number of objects in the cache, so we
            // should adjust our unconditional sampling accordingly.
            // Normally, we scan 10x per second, but we can simply lower
            // this to every few seconds, based on the SHARDS value.
            uint64_t time_between_probes_ms =
                shards_.scale / 10 * Duration::SECOND;
            if (access.timestamp_ms % time_between_probes_ms != 0) {
                return;
            }
            while (remove_expired_via_sampling(access))
                ;
        } else {
            if (!run_once && 10 % shards_.scale != 0) {
                LOGGER_WARN("10 does not divide nicely by scale %zu ",
                            shards_.scale);
                run_once = true;
            }
            uint64_t times_per_second = 10 / shards_.scale;
            // For unsampled SHARDS.
            for (uint64_t i = 0; i < times_per_second; ++i) {
                while (remove_expired_via_sampling(access))
                    ;
            }
        }
        // We mark the low-water mark here.
        std::function<double(double, double)> f = [](double old,
                                                     double new_) -> double {
            return std::isnan(old) ? new_ : std::max(old, new_);
        };
        statistics_.update_custom_metric(f, size_bytes_);
    }

public:
    RedisTTL(uint64_t const capacity_bytes, double const shards_sampling_ratio)
        : Accurate{capacity_bytes, shards_sampling_ratio},
          redis_sampler_{}
    {
    }

private:
    /* Keys for each DB loop. */
    constexpr static uint64_t ACTIVE_EXPIRE_CYCLE_KEYS_PER_LOOP = 20;
    /* % of stale keys after which we do extra efforts. */
    constexpr static uint64_t ACTIVE_EXPIRE_CYCLE_ACCEPTABLE_STALE = 10;
    // The threshold at which we decide an object is expiring 'soon' and
    // therefore should go into the tree.
    constexpr static uint64_t SOON_EXPIRING_THRESHOLD_MS = 1 * Duration::SECOND;
    // As per
    // [here](https://github.com/redis/redis/blob/unstable/src/expire.c),
    // the effort is in the range [0, 9] with a default value of 0. Note
    // that it is scaled from the user input, which is on the range [1, 10].
    constexpr static uint64_t EFFORT = 0;

    // These constants are as defined by Redis from the above parameters.
    constexpr static uint64_t NUMBER_SAMPLES =
        ACTIVE_EXPIRE_CYCLE_KEYS_PER_LOOP +
        ACTIVE_EXPIRE_CYCLE_KEYS_PER_LOOP / 4 * EFFORT;
    constexpr static double ACCEPTABLE_STALE_RATIO =
        (double)(ACTIVE_EXPIRE_CYCLE_ACCEPTABLE_STALE - EFFORT) / 100;

    RedisSampler redis_sampler_;
    // This tree only contains soon expiring objects for memory
    // efficiency (rather than containing all of the objects).
    std::multimap<double, uint64_t> ttl_queue_;
};
