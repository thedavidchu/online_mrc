/// TODO
/// 1. Test with real trace (how to get TTLs?)
/// 2. How to count miscounts?
/// 3. How to do certainty?
/// 4. This is really slow. Profile it to see how long the LRU stack is
///     taking...
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

#include "cache_metadata/cache_access.hpp"
#include "cache_metadata/cache_metadata.hpp"
#include "cache_statistics/cache_statistics.hpp"
#include "list/list.hpp"
#include "logger/logger.h"
#include "math/saturation_arithmetic.h"
#include "trace/reader.h"

using size_t = std::size_t;
using uint64_t = std::uint64_t;

enum class EvictionCause {
    LRU,
    TTL,
};

class EvictionPredictor {
public:
    EvictionPredictor(size_t const capacity, double const certainty)
        : capacity_(capacity),
          certainty_(certainty)
    {
    }

    /// @brief  Decide whether to store in the LRU queue.
    /// @param access: CacheAccess const & - cache access.
    /// @param dt: uint64_t const - time the cache has been alive.
    bool
    store_lru(CacheAccess const &access, uint64_t const dt) const
    {
        // What is the chance of being evicted by LRU?
        uint64_t should_have_evicted_bytes =
            correctly_evicted_bytes_ + wrongly_expired_bytes_;
        double eviction_rate = (double)should_have_evicted_bytes / dt;
        uint64_t time_until_eviction = capacity_ / eviction_rate;
        // NOTE This is an incredibly crude mechanism to eventually
        //      acheive the 'certainty' based on historical data.
        if (wrongly_expired_bytes_ > correctly_evicted_bytes_ * certainty_) {
            return true;
        }
        return access.ttl_ms >= time_until_eviction;
    }

    /// @brief  Decide whether to store in the TTL queue.
    /// @param access: CacheAccess const & - cache access.
    /// @param dt: uint64_t const - time the cache has been alive.
    bool
    store_ttl(CacheAccess const &access, uint64_t const dt) const
    {
        // What is the chance of being evicted by TTL?
        uint64_t should_have_expired_bytes =
            correctly_expired_bytes_ + wrongly_evicted_bytes_;
        double eviction_rate = (double)should_have_expired_bytes / dt;
        uint64_t time_until_eviction = capacity_ / eviction_rate;
        // NOTE This is an incredibly crude mechanism to eventually
        //      acheive the 'certainty' based on historical data.
        //      This may not even be the correct way to do it; we may
        //      want something closer to (abs(x - y) / max(x, y)).
        if (wrongly_evicted_bytes_ > correctly_expired_bytes_ * certainty_) {
            return true;
        }
        return access.ttl_ms <= time_until_eviction;
    }

    void
    update_correctly_evicted(size_t const bytes)
    {
        correctly_evicted_bytes_ += bytes;
        sq_corr_evict_ += bytes * bytes;
    }

    void
    update_correctly_expired(size_t const bytes)
    {
        correctly_expired_bytes_ += bytes;
        sq_corr_exp_ += bytes * bytes;
    }

    void
    update_wrongly_evicted(size_t const bytes)
    {
        wrongly_evicted_bytes_ += bytes;
        sq_wrong_evict_ += bytes * bytes;
    }

    void
    update_wrongly_expired(size_t const bytes)
    {
        wrongly_expired_bytes_ += bytes;
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
    double const certainty_;
    uint64_t correctly_evicted_bytes_ = 0;
    uint64_t correctly_expired_bytes_ = 0;
    uint64_t wrongly_evicted_bytes_ = 0;
    uint64_t wrongly_expired_bytes_ = 0;

    // These are for variance.
    size_t sq_corr_evict_ = 0;
    size_t sq_corr_exp_ = 0;
    size_t sq_wrong_evict_ = 0;
    size_t sq_wrong_exp_ = 0;
};

/// @brief  Remove a specific <key, value> pair from a multimap,
///         specifically the first instance of that pair.
template <typename K, typename V>
static bool
remove_multimap_kv(std::multimap<K, V> &me, K const k, V const v)
{
    for (auto it = me.lower_bound(k); it != me.upper_bound(k); ++it) {
        if (it->second == v) {
            me.erase(it);
            return true;
        }
    }
    return false;
}

class PredictiveCache {
private:
    void
    insert(uint64_t const key,
           uint64_t const size_bytes,
           uint64_t access_time_ms,
           uint64_t const expiration_time_ms)
    {
        map_.emplace(
            key,
            CacheMetadata{size_bytes, access_time_ms, expiration_time_ms});
        // TODO If we've filled the cache (i.e. steady-state) and have
        //      some wiggle room for uncertainty, then we can optionally
        //      not add to one of the caches.
        lru_cache_.access(key);
        ttl_cache_.emplace(expiration_time_ms, key);
        size_ += size_bytes;
        inserted_bytes_ += size_bytes;
    }

    bool
    update(uint64_t const key, uint64_t const access_time_ms)
    {
        auto &metadata = map_.at(key);
        // NOTE I do not allow the TTL to be updated after the first
        //      insertion. This is to simplify semantics.
        metadata.visit(access_time_ms, {});
        lru_cache_.access(key);
        return true;
    }

    void
    evict(uint64_t const victim_key, EvictionCause const cause)
    {
        bool erased_lru = false, erased_ttl = false;
        CacheMetadata &m = map_.at(victim_key);
        uint64_t sz_bytes = m.size_;
        uint64_t last_access = m.last_access_time_ms_;
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
            if (last_evicted_ <= last_access) {
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
        if (!erased_lru) {
            LOGGER_WARN("could not evict from LRU");
        }
        // Evict from TTL queue.
        erased_ttl = remove_multimap_kv(ttl_cache_, exp_tm, victim_key);
        if (!erased_ttl) {
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
    ensure_enough_room(size_t const nbytes)
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
            LOGGER_WARN("cannot possibly evict enough bytes: need to evict "
                        "%zu, but can only evict %zu",
                        nbytes,
                        capacity_);
            return false;
        }
        uint64_t evicted_bytes = 0;
        std::vector<uint64_t> victims;
        // Otherwise, make room in the cache for the new object.
        // TODO
        for (auto n = lru_cache_.begin(); n != lru_cache_.end(); n = n->r) {
            if (evicted_bytes >= nbytes) {
                break;
            }
            evicted_bytes += map_.at(n->key).size_;
            last_evicted_ =
                std::max(last_evicted_, map_.at(n->key).last_access_time_ms_);
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
        bool r = update(access.key, access.timestamp_ms);
        if (!r) {
            LOGGER_ERROR("could not update on hit");
        }
        statistics_.hit(access.size_bytes);
    }

    int
    miss(CacheAccess const &access)
    {
        if (!ensure_enough_room(access.size_bytes)) {
            LOGGER_WARN("not enough room to insert!");
            statistics_.miss(access.size_bytes);
            return -1;
        }
        uint64_t exp_tm = saturation_add(access.timestamp_ms,
                                         access.ttl_ms.value_or(UINT64_MAX));
        insert(access.key, access.size_bytes, access.timestamp_ms, exp_tm);
        statistics_.miss(access.size_bytes);
        return 0;
    }

public:
    PredictiveCache(size_t const capacity, double const certainty)
        : capacity_(capacity),
          predictor_(capacity, certainty)
    {
        assert(0 <= certainty && certainty <= 1.0);
    }

    int
    access(CacheAccess const &access)
    {
        int err = 0;
        current_time_ms_ = access.timestamp_ms;
        evict_expired_objects(access.timestamp_ms);
        if (map_.count(access.key)) {
            hit(access);
        } else {
            if ((err = miss(access))) {
                LOGGER_WARN("cannot handle miss");
                return -1;
            }
        }
        max_size_ = std::max(max_size_, size_);
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

    uint64_t current_time_ms_ = 0;
    // The maximum access time associated with any evicted object.
    uint64_t last_evicted_ = 0;

    // Maps key to [last access time, expiration time]
    std::unordered_map<uint64_t, CacheMetadata> map_;
    // Maps last access time to keys.
    List lru_cache_;
    // Maps expiration time to keys.
    std::multimap<uint64_t, uint64_t> ttl_cache_;
};

bool
test_lru()
{
    PredictiveCache p(2, 1.0);
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
    PredictiveCache p(2, 1.0);
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
           double const certainty)
{
    PredictiveCache p(capacity_bytes, certainty);
    LOGGER_TIMING("starting test_trace()");
    for (size_t i = 0; i < trace.size(); ++i) {
        int err = p.access(trace.get(i));
        if (err) {
            LOGGER_WARN("error...");
        }
    }
    LOGGER_TIMING("finished test_trace()");

    auto pred = p.predictor();
    std::cout << "> Capacity [B]: " << p.capacity()
              << " | Max Size [B]: " << p.max_size()
              << " | Correct Eviction [B]: " << pred.correctly_evicted_bytes_
              << " | Correct Expiration [B]: " << pred.correctly_expired_bytes_
              << " | Wrong Eviction [B]: " << pred.wrongly_evicted_bytes_
              << " | Wrong Expiration [B]: " << pred.wrongly_expired_bytes_
              << " | Miss Ratio: " << p.miss_rate() << std::endl;
    return true;
}

int
main(int argc, char *argv[])
{
    switch (argc) {
    case 1:
        assert(test_lru());
        assert(test_ttl());
        std::cout << "OK!" << std::endl;
        break;
    case 3:
        for (int i = 1; i < 11; ++i) {
            assert(test_trace(
                CacheAccessTrace(argv[1], parse_trace_format_string(argv[2])),
                i * (size_t)1 << 30,
                1.0));
        }
        std::cout << "OK!" << std::endl;
        break;
    default:
        std::cout << "Usage: predictor [<trace> <format>]" << std::endl;
        exit(1);
    }
    return 0;
}
