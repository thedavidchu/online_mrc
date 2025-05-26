#include "lib/predictive_lru_ttl_cache.hpp"
#include "arrays/is_last.h"
#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_metadata.hpp"
#include "cpp_lib/cache_statistics.hpp"
#include "cpp_lib/format_measurement.hpp"
#include "cpp_lib/util.hpp"
#include "cpp_struct/hash_list.hpp"
#include "lib/eviction_cause.hpp"
#include "lib/lifetime_cache.hpp"
#include "lib/lifetime_thresholds.hpp"
#include "lib/lru_ttl_cache.hpp"
#include "lib/prediction_tracker.hpp"
#include "logger/logger.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <glib.h>
#include <iostream>
#include <map>
#include <ostream>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
#include <vector>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

/// @note   An object is expired when its expiration time is past.
static inline bool
object_is_expired(uint64_t expiration_time, uint64_t current_time)
{
    // An object at its expiration time is still valid; past is invalid.
    return current_time > expiration_time;
}

bool
PredictiveCache::ok(bool const fatal) const
{
    bool ok = true;
    if (size_ > capacity_) {
        LOGGER_ERROR("size exceeds capacity");
        ok = false;
    }
    if (map_.size() < lru_cache_.size()) {
        // NOTE Because of the prediction, we can have fewer items in
        //      the LRU queue than in the cache.
        LOGGER_ERROR("mismatching map vs LRU size");
        ok = false;
    }
    if (map_.size() < ttl_cache_.size()) {
        // NOTE Because of the prediction, we can have fewer items in
        //      the TTL queue than in the cache.
        LOGGER_ERROR("mismatching map vs TTL size");
        ok = false;
    }
    if (map_.size() != 0 && size_ == 0) {
        // NOTE It's possible (but unlikely) that the cache is filled
        //      with zero-byte objects, so the number of objects is
        //      non-zero but the size of the cache is zero. That's why I
        //      don't error on this case explicitly.
        LOGGER_WARN("all zero-sized objects in cache");
        // TODO - make this not an error.
        ok = false;
    }
    if (map_.size() == 0 && size_ != 0) {
        LOGGER_ERROR("zero objects but non-zero cache size");
        ok = false;
    }

    if (fatal && !ok) {
        // The assertion outputs a convenient error message.
        assert(ok && "FATAL: not OK!");
        std::exit(1);
    }
    return ok;
}

/// @brief  Insert an object into the cache.
void
PredictiveCache::insert(CacheAccess const &access)
{
    statistics_.insert(access.value_size_b);
    double const ttl_ms = access.ttl_ms;
    map_.emplace(access.key, CacheMetadata{access});
    auto r = lifetime_cache_.thresholds();
    if (r.first != UINT64_MAX && ttl_ms >= r.first) {
        pred_tracker.record_store_lru();
        lru_cache_.access(access.key);
    }
    if (r.second != 0 && ttl_ms <= r.second) {
        pred_tracker.record_store_ttl();
        ttl_cache_.emplace(access.expiration_time_ms(), access.key);
    }
    size_ += access.value_size_b;
}

/// @brief  Process an access to an item in the cache.
void
PredictiveCache::update(CacheAccess const &access, CacheMetadata &metadata)
{
    size_ += access.value_size_b - metadata.size_;
    statistics_.update(metadata.size_, access.value_size_b);
    metadata.visit_without_ttl_refresh(access);
    if (lifetime_cache_.mode() == LifeTimeCacheMode::LifeTime) {
        return;
    }
    double const ttl_ms = metadata.ttl_ms(access.timestamp_ms);
    auto r = lifetime_cache_.thresholds();
    if (r.first != UINT64_MAX && ttl_ms >= r.first) {
        pred_tracker.record_store_lru();
        lru_cache_.access(access.key);
    } else {
        lru_cache_.remove(access.key);
    }
    if (r.second != 0 && ttl_ms <= r.second) {
        // Even if we don't re-insert into the TTL queue, we still
        // want to mark it as stored.
        pred_tracker.record_store_ttl();
        // Only insert the TTL if it hasn't been inserted already.
        if (find_multimap_kv(ttl_cache_,
                             (double)metadata.expiration_time_ms_,
                             access.key) == ttl_cache_.end()) {
            ttl_cache_.emplace(metadata.expiration_time_ms_, access.key);
        }
    } else {
        remove_multimap_kv(ttl_cache_,
                           metadata.expiration_time_ms_,
                           access.key);
    }
}

/// @brief  Evict an object in the cache (either due to the eviction
///         policy or TTL expiration).
void
PredictiveCache::remove(uint64_t const victim_key,
                        EvictionCause const cause,
                        CacheAccess const *const current_access)
{
    ok(true);
    CacheMetadata &m = map_.at(victim_key);
    uint64_t sz_bytes = m.size_;
    double exp_tm = m.expiration_time_ms_;

    // Update metadata tracking
    switch (cause) {
    case EvictionCause::LRU:
        assert(current_access != NULL);
        statistics_.lru_evict(m.size_, m.ttl_ms(current_access->timestamp_ms));
        if (statistics_.current_time_ms_ <= exp_tm) {
            // Not yet expired.
            pred_tracker.update_correctly_evicted(sz_bytes);
        } else {
            pred_tracker.update_wrongly_evicted(sz_bytes);
        }
        break;
    case EvictionCause::TTL:
        statistics_.ttl_expire(m.size_);
        if (oracle_.get(victim_key)) {
            pred_tracker.update_correctly_expired(sz_bytes);
        } else {
            pred_tracker.update_wrongly_expired(sz_bytes);
        }
        break;
    case EvictionCause::VolatileTTL:
        assert(current_access != NULL);
        statistics_.ttl_evict(m.size_, m.ttl_ms(current_access->timestamp_ms));
        // NOTE This isn't exactly the correct classification...
        //      It wasn't expired, but rather it was the soonest-to-expire.
        pred_tracker.update_wrongly_expired(sz_bytes);
        break;
    case EvictionCause::AccessExpired:
        assert(current_access != NULL);
        statistics_.lazy_expire(m.size_,
                                m.ttl_ms(current_access->timestamp_ms));
        // NOTE This isn't exactly the correct classification...
        //      It wasn't evicted by LRU, it was evicted by
        //      reaccessing an expired object. However, it would
        //      have been in the LRU queue, which is why I chose this.
        pred_tracker.update_wrongly_evicted(sz_bytes);
        break;
    case EvictionCause::NoRoom:
        assert(current_access != NULL);
        statistics_.no_room_evict(m.size_,
                                  m.ttl_ms(current_access->timestamp_ms));
        // NOTE This isn't exactly the correct classification...
        //      It wasn't evicted by LRU, it was evicted by
        //      running out of space in the cache for a re-accessed item.
        pred_tracker.update_correctly_evicted(sz_bytes);
        break;
    case EvictionCause::Sampling:
        statistics_.sampling_remove(m.size_);
        break;
    default:
        assert(0 && "impossible");
    }

    size_ -= sz_bytes;
    map_.erase(victim_key);
    lru_cache_.remove(victim_key);
    remove_multimap_kv(ttl_cache_, exp_tm, victim_key);
}

void
PredictiveCache::evict_expired_objects(uint64_t const current_time_ms)
{
    std::vector<uint64_t> victims;
    for (auto [exp_tm, key] : ttl_cache_) {
        if (!object_is_expired(exp_tm, current_time_ms)) {
            break;
        }
        victims.push_back(key);
    }
    // One cannot erase elements from a multimap while also iterating!
    for (auto victim : victims) {
        remove(victim, EvictionCause::TTL, nullptr);
    }
}

/// @return number of bytes evicted.
uint64_t
PredictiveCache::evict_from_lru(uint64_t const target_bytes,
                                CacheAccess const &access)
{
    ok(true);
    uint64_t const ignored_key = access.key;
    uint64_t evicted_bytes = 0;
    std::vector<uint64_t> victims;
    for (auto n : lru_cache_) {
        if (evicted_bytes >= target_bytes) {
            break;
        }
        if (n->key == ignored_key) {
            continue;
        }
        auto &m = map_.at(n->key);
        evicted_bytes += m.size_;
        victims.push_back(n->key);
    }
    // One cannot evict elements from the map one is iterating over.
    for (auto v : victims) {
        remove(v, EvictionCause::LRU, &access);
    }
    return evicted_bytes;
}

uint64_t
PredictiveCache::evict_smallest_ttl(uint64_t const target_bytes,
                                    CacheAccess const &access)
{
    uint64_t const ignored_key = access.key;
    uint64_t evicted_bytes = 0;
    std::vector<uint64_t> victims;
    for (auto [tm, key] : ttl_cache_) {
        if (evicted_bytes >= target_bytes) {
            break;
        }
        if (key == ignored_key) {
            continue;
        }
        auto &m = map_.at(key);
        evicted_bytes += m.size_;
        victims.push_back(key);
    }
    // One cannot evict elements from the map one is iterating over.
    for (auto v : victims) {
        remove(v, EvictionCause::VolatileTTL, &access);
    }
    return evicted_bytes;
}

bool
PredictiveCache::ensure_enough_room(size_t const old_nbytes,
                                    CacheAccess const &access)
{
    size_t const new_nbytes = access.key_size_b + access.value_size_b;
    assert(size_ <= capacity_);
    // We already have enough room if we're not increasing the data.
    if (old_nbytes >= new_nbytes) {
        return true;
    }
    size_t const nbytes = new_nbytes - old_nbytes;
    // We can't possibly fit the new object into the cache!
    // A side-effect is that in this case, we don't flush our cache
    // for no reason.
    if (new_nbytes > capacity_) {
        if (DEBUG) {
            LOGGER_WARN("not enough capacity (%zu) for object (%zu)",
                        capacity_,
                        nbytes);
        }
        return false;
    }
    // Check that the required bytes to free is greater than zero.
    if (nbytes <= capacity_ - size_) {
        return true;
    }
    uint64_t const reqd_b = nbytes - (capacity_ - size_);
    uint64_t const lru_evicted_b = evict_from_lru(reqd_b, access);
    if (lru_evicted_b >= reqd_b) {
        return true;
    }
    // Evict from TTL queue as well, since the LRU queue may not
    // have enough elements to create enough room, since it doesn't
    // have all the elements in it.
    uint64_t const ttl_evicted_b =
        evict_smallest_ttl(reqd_b - lru_evicted_b, access);
    if (lru_evicted_b + ttl_evicted_b >= reqd_b) {
        return true;
    }
    // This is an error: it means that elements are in neither the TTL
    // nor the LRU queue.
    LOGGER_ERROR("could not evict enough from cache");
    return false;
}

void
PredictiveCache::evict_expired_accessed_object(CacheAccess const &access)
{
    remove(access.key, EvictionCause::AccessExpired, &access);
}

void
PredictiveCache::evict_too_big_accessed_object(CacheAccess const &access)
{
    remove(access.key, EvictionCause::NoRoom, &access);
}

bool
PredictiveCache::is_expired(CacheAccess const &access,
                            CacheMetadata const &metadata)
{
    return object_is_expired(metadata.expiration_time_ms_, access.timestamp_ms);
}

void
PredictiveCache::hit(CacheAccess const &access)
{
    auto &metadata = map_.at(access.key);
    if (!ensure_enough_room(metadata.size_, access)) {
        statistics_.skip(access.value_size_b);
        evict_too_big_accessed_object(access);
        if (DEBUG) {
            LOGGER_WARN("too big updated object");
        }
        return;
    }
    update(access, metadata);
}

bool
PredictiveCache::miss(CacheAccess const &access)
{
    if (!ensure_enough_room(0, access)) {
        if (DEBUG) {
            LOGGER_WARN("not enough room to insert!");
        }
        statistics_.skip(access.value_size_b);
        return false;
    }
    insert(access);
    return true;
}

/// @note   The {ttl,lru}_only parameters are deprecated. This wasn't
///         a particularly good idea nor was it particularly useful.
/// @param  capacity: size_t - The capacity of the cache in bytes.
/// @param  {lower,upper}_ratio: double [0.0, 1.0] - The
///         ratio thresholds for prediction.
/// @param  record_reaccess: bool
///         If true, then record the lifetime as the time between
///         either LRU evictions or reaccess.
///         Otherwise, record the lifetime as the time between
///         insertion and LRU eviction.
/// @param  repredict_reaccess: bool
///         If true, then re-predict the queues on each reaccess.
///         Otherwise, predict the queue only on insertion.
PredictiveCache::PredictiveCache(size_t const capacity,
                                 double const lower_ratio,
                                 double const upper_ratio,
                                 LifeTimeCacheMode const cache_mode,
                                 std::map<std::string, std::string> kwargs)
    : capacity_(capacity),
      lifetime_cache_mode_(cache_mode),
      lifetime_cache_(capacity, lower_ratio, upper_ratio, cache_mode),
      lifetime_thresholds_(lower_ratio, upper_ratio),
      oracle_(capacity),
      kwargs_(kwargs)
{
}

void
PredictiveCache::start_simulation()
{
    statistics_.start_simulation();
    oracle_.start_simulation();
}

void
PredictiveCache::end_simulation()
{
    statistics_.end_simulation();
    oracle_.end_simulation();
}

int
PredictiveCache::access(CacheAccess const &access)
{
    ok(true);
    g_assert_cmpuint(size_, ==, statistics_.size_);
    statistics_.time(access.timestamp_ms);
    evict_expired_objects(access.timestamp_ms);
    lifetime_cache_.access(access);
    oracle_.access(access);
    if (map_.count(access.key)) {
        auto &metadata = map_.at(access.key);
        if (!is_expired(access, metadata)) {
            hit(access);
            return 0;
        } else {
            evict_expired_accessed_object(access);
        }
    }

    bool ok = miss(access);
    if (!ok) {
        if (DEBUG) {
            LOGGER_WARN("cannot handle miss");
        }
        return -1;
    }
    return 0;
}

size_t
PredictiveCache::size() const
{
    return size_;
}

size_t
PredictiveCache::capacity() const
{
    return capacity_;
}

LifeTimeCacheMode
PredictiveCache::lifetime_cache_mode() const
{
    return lifetime_cache_mode_;
}

CacheMetadata const *
PredictiveCache::get(uint64_t const key)
{
    if (map_.count(key)) {
        return &map_.at(key);
    }
    return nullptr;
}

void
PredictiveCache::print()
{
    std::cout << "> PredictiveCache(sz: " << size_ << ", cap: " << capacity_
              << ")\n";
    std::cout << "> \tLRU: ";
    for (auto n : lru_cache_) {
        std::cout << n->key << ", ";
    }
    std::cout << "\n";
    std::cout << "> \tTTL: ";
    for (auto [tm, key] : ttl_cache_) {
        std::cout << key << "@" << tm << ", ";
    }
    std::cout << "\n";
}

PredictionTracker const &
PredictiveCache::predictor() const
{
    return pred_tracker;
}

CacheStatistics const &
PredictiveCache::statistics() const
{
    return statistics_;
}

void
PredictiveCache::print_statistics(
    std::ostream &ostrm,
    std::map<std::string, std::string> extras) const
{
    auto r = lifetime_cache_.thresholds();
    ostrm << "> {\"Capacity [B]\": " << format_memory_size(capacity_)
          << ", \"Lower Ratio\": " << lifetime_cache_.lower_ratio()
          << ", \"Upper Ratio\": " << lifetime_cache_.upper_ratio()
          << ", \"Lifetime Cache Mode\": \""
          << LifeTimeCacheMode__str(lifetime_cache_.mode())
          << "\", \"CacheStatistics\": " << statistics_.json()
          << ", \"PredictionTracker\": " << pred_tracker.json()
          << ", \"Lifetime Thresholds\": " << lifetime_thresholds_.json()
          << ", \"Threshold Refreshes [#]\": "
          << format_engineering(lifetime_cache_.refreshes())
          << ", \"Samples Since Threshold Refresh [#]\": "
          << format_engineering(lifetime_cache_.since_refresh())
          << ", \"LRU Lifetime Evictions [#]\": "
          << format_engineering(lifetime_cache_.evictions())
          << ", \"Lower Threshold [ms]\": " << format_time(r.first)
          << ", \"Upper Threshold [ms]\": " << format_time(r.second)
          << ", \"Lifetime Cache\": " << lifetime_cache_.json()
          << ", \"Kwargs\": " << map2str(kwargs_, true)
          << ", \"Extras\": " << map2str(extras, false);
    ostrm << "}" << std::endl;
}
