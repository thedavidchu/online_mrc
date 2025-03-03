/// TODO
/// 1. Test with real trace (how to get TTLs?)
/// 2. How to count miscounts?
/// 3. How to do certainty?
/// 4. This is really slow. Profile it to see how long the LRU stack is
///     taking...
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cache_metadata/cache_access.hpp"
#include "cache_metadata/cache_metadata.hpp"
#include "cache_statistics/cache_statistics.hpp"
#include "list/list.hpp"
#include "logger/logger.h"
#include "math/saturation_arithmetic.h"
#include "trace/reader.h"

#include "list/lifetime_thresholds.hpp"

using size_t = std::size_t;
using uint64_t = std::uint64_t;

bool const DEBUG = false;

enum class EvictionCause {
    LRU,
    TTL,
};

class LifeTimeCache {
public:
    LifeTimeCache(size_t const capacity, double const uncertainty)
        : capacity_(capacity),
          thresholds_(uncertainty)
    {
    }

    bool
    contains(uint64_t const key)
    {
        // This is just to make sure I really do return 'true' or 'false'.
        return map_.count(key) ? true : false;
    }

    void
    access(CacheAccess const &access)
    {
        if (!first_time_ms_.has_value()) {
            first_time_ms_ = access.timestamp_ms;
        }
        current_time_ms_ = access.timestamp_ms;

        if (map_.count(access.key)) {
            map_.at(access.key).visit(access.timestamp_ms, {});
        } else {
            if (access.size_bytes > capacity_) {
                // Skip objects that are too large for the cache!
                return;
            }
            while (size_ + access.size_bytes > capacity_) {
                auto x = lru_cache_.remove_head();
                auto &node = map_.at(x->key);
                size_ -= node.size_;
                thresholds_.register_cache_eviction(current_time_ms_ -
                                                        node.insertion_time_ms_,
                                                    node.size_);
                // Erase everything
                map_.erase(x->key);
                std::free(x);
            }
            map_.emplace(access.key, CacheMetadata{access});
        }
    }

    std::pair<uint64_t, uint64_t>
    get_time_thresholds()
    {
        auto r = thresholds_.get_thresholds();
        assert(r.first <= r.second);
        return r;
    }

private:
    // Maximum number of bytes in the cache.
    size_t const capacity_;
    // Number of bytes in the current cache.
    size_t size_ = 0;

    std::optional<uint64_t> first_time_ms_ = std::nullopt;
    uint64_t current_time_ms_ = 0;

    // Maps key to [last access time, expiration time]
    std::unordered_map<uint64_t, CacheMetadata> map_;
    // Maps last access time to keys.
    List lru_cache_;

    LifeTimeThresholds thresholds_;
};

class EvictionPredictor {
public:
    EvictionPredictor(size_t const capacity,
                      double const ttl_only,
                      double const lru_only)
        : capacity_(capacity),
          ttl_only_(ttl_only),
          lru_only_(lru_only)
    {
    }

    /// @brief  Record a store in the LRU queue.
    /// @param access: CacheAccess const & - cache access.
    /// @param dt: uint64_t const - time the cache has been alive.
    bool
    record_store_lru()
    {
        ++guessed_lru_;
        return true;
    }

    /// @brief  Record a store in the TTL queue.
    /// @param access: CacheAccess const & - cache access.
    /// @param dt: uint64_t const - time the cache has been alive.
    bool
    record_store_ttl()
    {
        ++guessed_ttl_;
        return true;
    }

    void
    update_correctly_evicted(size_t const bytes)
    {
        correctly_evicted_bytes_ += bytes;
        correctly_evicted_ops_ += 1;
        sq_corr_evict_ += bytes * bytes;
    }

    void
    update_correctly_expired(size_t const bytes)
    {
        correctly_expired_bytes_ += bytes;
        correctly_expired_ops_ += 1;
        sq_corr_exp_ += bytes * bytes;
    }

    void
    update_wrongly_evicted(size_t const bytes)
    {
        wrongly_evicted_bytes_ += bytes;
        wrongly_evicted_ops_ += 1;
        sq_wrong_evict_ += bytes * bytes;
    }

    void
    update_wrongly_expired(size_t const bytes)
    {
        wrongly_expired_bytes_ += bytes;
        wrongly_expired_ops_ += 1;
        sq_wrong_exp_ += bytes * bytes;
    }

    // TODO Make these private? There's no real point other than safety,
    //      which is a darn good reason. But I'd just make a setter
    //      method anyways, which would be a waste of time.
    size_t const capacity_;
    // NOTE The certainty can also be made specific to the TTL policy
    //      versus the regular eviction policy. For example, we may want
    //      to bias toward a higher chance of including an object in the
    //      TTL queue than the LRU queue.
    double const ttl_only_;
    double const lru_only_;

    // LRU Oracle
    size_t lru_size_;
    List lru_list_;

    uint64_t guessed_lru_ = 0;
    uint64_t guessed_ttl_ = 0;

    uint64_t correctly_evicted_bytes_ = 0;
    uint64_t correctly_expired_bytes_ = 0;
    uint64_t wrongly_evicted_bytes_ = 0;
    uint64_t wrongly_expired_bytes_ = 0;

    // Track the number of correct/wrong operations
    uint64_t correctly_evicted_ops_ = 0;
    uint64_t correctly_expired_ops_ = 0;
    uint64_t wrongly_evicted_ops_ = 0;
    uint64_t wrongly_expired_ops_ = 0;

    // These are for variance.
    size_t sq_corr_evict_ = 0;
    size_t sq_corr_exp_ = 0;
    size_t sq_wrong_evict_ = 0;
    size_t sq_wrong_exp_ = 0;
};

template <typename K, typename V>
static auto
find_multimap_kv(std::multimap<K, V> &me, K const k, V const v)
{
    for (auto it = me.lower_bound(k); it != me.upper_bound(k); ++it) {
        if (it->second == v) {
            return it;
        }
    }
    return me.end();
}

/// @brief  Remove a specific <key, value> pair from a multimap,
///         specifically the first instance of that pair.
template <typename K, typename V>
static bool
remove_multimap_kv(std::multimap<K, V> &me, K const k, V const v)
{
    auto it = find_multimap_kv(me, k, v);
    if (it != me.end()) {
        me.erase(it);
        return true;
    }
    return false;
}

class PredictiveCache {
private:
    /// @brief  Return the time the cache has been running.
    size_t
    uptime(CacheAccess const &access)
    {
        return access.timestamp_ms -
               first_time_ms_.value_or(access.timestamp_ms);
    }

    /// @brief  Insert an object into the cache.
    void
    insert(CacheAccess const &access)
    {
        ++num_insertions_;
        uint64_t const ttl_ms = access.ttl_ms.value_or(UINT64_MAX);
        uint64_t const expiration_time_ms =
            saturation_add(access.timestamp_ms,
                           access.ttl_ms.value_or(UINT64_MAX));
        if (!first_time_ms_.has_value()) {
            first_time_ms_ = access.timestamp_ms;
        }
        map_.emplace(access.key,
                     CacheMetadata{access.size_bytes,
                                   access.timestamp_ms,
                                   expiration_time_ms});
        if (num_insertions_ % (1 << 20) == 0) {
            lifetime_cache_.get_time_thresholds();
        }
        auto r = lifetime_cache_.get_time_thresholds();
        if (ttl_ms >= r.first) {
            predictor_.record_store_lru();
            lru_cache_.access(access.key);
        }
        if (ttl_ms <= r.second) {
            predictor_.record_store_ttl();
            ttl_cache_.emplace(expiration_time_ms, access.key);
        }
        size_ += access.size_bytes;
        inserted_bytes_ += access.size_bytes;
    }

    /// @brief  Process an access to an item in the cache.
    bool
    update(CacheAccess const &access)
    {
        ++num_updates_;
        auto &metadata = map_.at(access.key);
        metadata.visit(access.timestamp_ms, {});
        // We want the new TTL after the access (above).
        uint64_t const ttl_ms = metadata.ttl_ms();

        auto r = lifetime_cache_.get_time_thresholds();
        if (ttl_ms >= r.first) {
            predictor_.record_store_lru();
            lru_cache_.access(access.key);
        }
        if (ttl_ms <= r.second) {
            // Even if we don't re-insert into the TTL queue, we still
            // want to mark it as stored.
            predictor_.record_store_ttl();
            // Only insert the TTL if it hasn't been inserted already.
            if (find_multimap_kv(ttl_cache_,
                                 metadata.expiration_time_ms_,
                                 access.key) == ttl_cache_.end()) {
                ttl_cache_.emplace(metadata.expiration_time_ms_, access.key);
            }
        }
        return true;
    }

    /// @brief  Evict an object in the cache (either due to the eviction
    ///         policy or TTL expiration).
    void
    evict(uint64_t const victim_key, EvictionCause const cause)
    {
        bool erased_lru = false, erased_ttl = false;
        CacheMetadata &m = map_.at(victim_key);
        uint64_t sz_bytes = m.size_;
        uint64_t exp_tm = m.expiration_time_ms_;

        // Update metadata tracking
        switch (cause) {
        case EvictionCause::LRU:
            evicted_bytes_ += sz_bytes;
            if (current_time_ms_ <= exp_tm) {
                // Not yet expired.
                predictor_.update_correctly_evicted(sz_bytes);
            } else {
                predictor_.update_wrongly_evicted(sz_bytes);
            }
            break;
        case EvictionCause::TTL:
            expired_bytes_ += sz_bytes;
            if (lifetime_cache_.contains(victim_key)) {
                predictor_.update_correctly_expired(sz_bytes);
            } else {
                predictor_.update_wrongly_expired(sz_bytes);
            }
            break;
        default:
            assert(0 && "impossible");
        }

        // Evict from map.
        map_.erase(victim_key);
        size_ -= sz_bytes;
        // Evict from LRU queue.
        erased_lru = lru_cache_.remove(victim_key);
        if (DEBUG && !erased_lru) {
            LOGGER_WARN("could not evict from LRU");
        }
        // Evict from TTL queue.
        erased_ttl = remove_multimap_kv(ttl_cache_, exp_tm, victim_key);
        if (DEBUG && !erased_ttl) {
            LOGGER_WARN("could not evict from TTL");
        }
    }

    void
    evict_expired_objects(uint64_t const current_time_ms)
    {
        std::vector<uint64_t> victims;
        for (auto [exp_tm, key] : ttl_cache_) {
            if (exp_tm >= current_time_ms) {
                break;
            }
            victims.push_back(key);
        }
        // One cannot erase elements from a multimap while also iterating!
        for (auto victim : victims) {
            evict(victim, EvictionCause::TTL);
        }
    }

    bool
    ensure_enough_room(size_t const nbytes, List &lru_cache)
    {
        // NOTE I need to check this for the empty cache because
        //      otherwise, we skip right over the for-loop and return
        //      a spurious false!
        if (nbytes <= capacity_ - size_) {
            return true;
        }
        // We can't possibly fit the new object into the cache!
        // A side-effect is that in this case, we don't flush our cache
        // for no reason.
        if (nbytes > capacity_) {
            if (DEBUG) {
                LOGGER_WARN("cannot possibly evict enough bytes: need to evict "
                            "%zu, but can only evict %zu",
                            nbytes,
                            capacity_);
            }
            return false;
        }
        uint64_t evicted_bytes = 0;
        std::vector<uint64_t> victims;
        // Otherwise, make room in the cache for the new object.
        for (auto n = lru_cache.begin(); n != lru_cache.end(); n = n->r) {
            if (evicted_bytes >= nbytes) {
                break;
            }
            auto &m = map_.at(n->key);
            evicted_bytes += m.size_;
            victims.push_back(n->key);
        }
        // One cannot evict elements from the map one is iterating over.
        for (auto v : victims) {
            evict(v, EvictionCause::LRU);
        }
        return true;
    }

    void
    hit(CacheAccess const &access)
    {
        bool r = update(access);
        if (DEBUG && !r) {
            LOGGER_ERROR("could not update on hit");
        }
        statistics_.hit(access.size_bytes);
    }

    int
    miss(CacheAccess const &access)
    {
        if (!ensure_enough_room(access.size_bytes, lru_cache_)) {
            if (DEBUG) {
                LOGGER_WARN("not enough room to insert!");
            }
            statistics_.miss(access.size_bytes);
            return -1;
        }
        insert(access);
        statistics_.miss(access.size_bytes);
        return 0;
    }

public:
    /// @note   The {ttl,lru}_only parameters are deprecated. This wasn't
    ///         a particularly good idea nor was it particularly useful.
    PredictiveCache(size_t const capacity,
                    double const ttl_only,
                    double const lru_only)
        : capacity_(capacity),
          predictor_(capacity, ttl_only, lru_only),
          lifetime_cache_(capacity, 0.25)
    {
    }

    size_t
    uptime() const
    {
        return current_time_ms_ - first_time_ms_.value_or(current_time_ms_);
    }

    int
    access(CacheAccess const &access)
    {
        int err = 0;
        current_time_ms_ = access.timestamp_ms;
        evict_expired_objects(access.timestamp_ms);
        lifetime_cache_.access(access);
        if (map_.count(access.key)) {
            hit(access);
        } else {
            if ((err = miss(access))) {
                if (DEBUG) {
                    LOGGER_WARN("cannot handle miss");
                }
                return -1;
            }
        }
        max_size_ = std::max(max_size_, size_);
        max_unique_ = std::max(max_unique_, map_.size());
        return 0;
    }

    size_t
    size() const
    {
        return size_;
    }

    size_t
    max_size() const
    {
        return max_size_;
    }

    size_t
    max_unique() const
    {
        return max_unique_;
    }

    size_t
    num_insertions() const
    {
        return num_insertions_;
    }

    size_t
    num_updates() const
    {
        return num_updates_;
    }

    size_t
    capacity() const
    {
        return capacity_;
    }

    CacheMetadata const *
    get(uint64_t const key)
    {
        if (map_.count(key)) {
            return &map_.at(key);
        }
        return nullptr;
    }

    void
    print()
    {
        std::cout << "> PredictiveCache(sz: " << size_ << ", cap: " << capacity_
                  << ")\n";
        std::cout << "> \tLRU: ";
        for (auto n = lru_cache_.begin(); n != lru_cache_.end(); n = n->r) {
            std::cout << n->key << ", ";
        }
        std::cout << "\n";
        std::cout << "> \tTTL: ";
        for (auto [tm, key] : ttl_cache_) {
            std::cout << key << "@" << tm << ", ";
        }
        std::cout << "\n";
    }

    void
    print_statistics() const
    {
        statistics_.print("PredictiveLRU", size_);
    }

    double
    miss_rate() const
    {
        return statistics_.miss_rate();
    }

    EvictionPredictor const &
    predictor() const
    {
        return predictor_;
    }

private:
    // Maximum number of bytes in the cache.
    size_t capacity_;
    // Number of bytes in the current cache.
    size_t size_ = 0;
    size_t max_size_ = 0;
    size_t max_unique_ = 0;
    size_t num_insertions_ = 0;
    size_t num_updates_ = 0;

    // Number of bytes inserted into the cache.
    size_t inserted_bytes_ = 0;
    // Number of bytes evicted by ordering algorithm.
    size_t evicted_bytes_ = 0;
    // Number of bytes expired by TTLs.
    size_t expired_bytes_ = 0;
    // Statistics related to prediction.
    EvictionPredictor predictor_;
    // Statistics related to cache performance.
    CacheStatistics statistics_;

    std::optional<uint64_t> first_time_ms_ = std::nullopt;
    uint64_t current_time_ms_ = 0;

    // Maps key to [last access time, expiration time]
    std::unordered_map<uint64_t, CacheMetadata> map_;
    // Maps last access time to keys.
    List lru_cache_;

    // Maps expiration time to keys.
    std::multimap<uint64_t, uint64_t> ttl_cache_;

public:
    // Real (or SHARDS-ified) LRU cache to monitor the lifetime of elements.
    LifeTimeCache lifetime_cache_;
};

bool
test_lru()
{
    PredictiveCache p(2, 0.0, INFINITY);
    CacheAccess accesses[] = {CacheAccess{0, 0, 1, 10},
                              CacheAccess{1, 1, 1, 10},
                              CacheAccess{2, 2, 1, 10}};
    // Test initial state.
    assert(p.size() == 0);
    assert(p.get(0) == nullptr);
    assert(p.get(1) == nullptr);
    assert(p.get(2) == nullptr);
    // Test first state.
    p.access(accesses[0]);
    assert(p.size() == 1);
    assert(p.get(0) != nullptr);
    assert(p.get(1) == nullptr);
    assert(p.get(2) == nullptr);

    // Test second state.
    p.access(accesses[1]);
    assert(p.size() == 2);
    assert(p.get(0) != nullptr);
    assert(p.get(1) != nullptr);
    assert(p.get(2) == nullptr);

    // Test third state.
    p.access(accesses[2]);
    assert(p.size() == 2);
    assert(p.get(0) == nullptr);
    assert(p.get(1) != nullptr);
    assert(p.get(2) != nullptr);

    return true;
}

bool
test_ttl()
{
    PredictiveCache p(2, 0.0, INFINITY);
    CacheAccess accesses[] = {CacheAccess{0, 0, 1, 1},
                              CacheAccess{1001, 1, 1, 10}};
    // Test initial state.
    assert(p.size() == 0);
    assert(p.get(0) == nullptr);
    assert(p.get(1) == nullptr);
    p.print();
    // Test first state.
    p.access(accesses[0]);
    assert(p.size() == 1);
    assert(p.get(0) != nullptr);
    assert(p.get(1) == nullptr);
    p.print();
    // Test second state.
    p.access(accesses[1]);
    assert(p.size() == 1);
    assert(p.get(0) == nullptr);
    assert(p.get(1) != nullptr);
    p.print();
    return true;
}

bool
test_trace(CacheAccessTrace const &trace,
           size_t const capacity_bytes,
           double const ttl_only,
           double const lru_only)
{
    PredictiveCache p(capacity_bytes, ttl_only, lru_only);
    LOGGER_TIMING("starting test_trace()");
    for (size_t i = 0; i < trace.size(); ++i) {
        int err = p.access(trace.get(i));
        if (DEBUG && err) {
            LOGGER_WARN("error...");
        }
    }
    LOGGER_TIMING("finished test_trace()");

    auto r = p.lifetime_cache_.get_time_thresholds();
    auto pred = p.predictor();
    std::cout << "> {\"Capacity [B]\": " << p.capacity()
              << ", \"Max Size [B]\": " << p.max_size()
              << ", \"Max Unique Objects\": " << p.max_unique()
              << ", \"Uptime [ms]\": " << p.uptime()
              << ", \"Number of Insertions\": " << p.num_insertions()
              << ", \"Number of Updates\": " << p.num_updates()
              << ", \"TTL Threshold\": " << ttl_only
              << ", \"LRU Threshold\": " << lru_only
              << ", \"Guessed LRU\": " << pred.guessed_lru_
              << ", \"Guessed TTL\": " << pred.guessed_ttl_
              << ", \"Correct Eviction Ops\": " << pred.correctly_evicted_ops_
              << ", \"Correct Expiration Ops\": " << pred.correctly_expired_ops_
              << ", \"Wrong Eviction Ops\": " << pred.wrongly_evicted_ops_
              << ", \"Wrong Expiration Ops\": " << pred.wrongly_expired_ops_
              << ", \"Correct Eviction [B]\": " << pred.correctly_evicted_bytes_
              << ", \"Correct Expiration [B]\": "
              << pred.correctly_expired_bytes_
              << ", \"Wrong Eviction [B]\": " << pred.wrongly_evicted_bytes_
              << ", \"Wrong Expiration [B]\": " << pred.wrongly_expired_bytes_
              << ", \"Miss Ratio\": " << p.miss_rate()
              << ", \"Lower Threshold\": " << r.first
              << ", \"Upper Threshold\": " << r.second << "}" << std::endl;
    return true;
}

int
main(int argc, char *argv[])
{
    switch (argc) {
    case 1:
        assert(test_lru());
        std::cout << "---\n";
        assert(test_ttl());
        std::cout << "OK!" << std::endl;
        break;
    case 3: {
        size_t sizes[] = {(size_t)1 << 30,
                          3 * (size_t)1 << 30,
                          6 * (size_t)1 << 30};

        for (auto cap : sizes) {
            assert(test_trace(
                CacheAccessTrace(argv[1], parse_trace_format_string(argv[2])),
                cap,
                0.0,
                0.0));
        }
        std::cout << "OK!" << std::endl;
        break;
    }
    default:
        std::cout << "Usage: predictor [<trace> <format>]" << std::endl;
        exit(1);
    }
    return 0;
}
