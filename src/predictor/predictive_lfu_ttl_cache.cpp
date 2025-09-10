#include "lib/predictive_lfu_ttl_cache.hpp"
#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_predictive_metadata.hpp"
#include "cpp_lib/cache_statistics.hpp"
#include "cpp_lib/format_measurement.hpp"
#include "cpp_lib/util.hpp"
#include "cpp_struct/hash_list.hpp"
#include "lib/eviction_cause.hpp"
#include "lib/lifetime_thresholds.hpp"
#include "lib/prediction_tracker.hpp"
#include "logger/logger.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <glib.h>
#include <iostream>
#include <map>
#include <ostream>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <tuple>
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
PredictiveLFUCache::ok(bool const fatal) const
{
    bool ok = true;
    if (size_ > capacity_) {
        LOGGER_ERROR("size exceeds capacity");
        ok = false;
    }
    if (lfu_cache_.size() != nr_lfu_buckets_) {
        LOGGER_ERROR("wrong number of LFU buckets: %zu vs 1",
                     lfu_cache_.size());
        ok = false;
    }
    if (map_.size() < lfu_nr_obj_) {
        LOGGER_ERROR("mismatching map vs LFU # obj");
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
    if (lfu_size_ > size_ || ttl_size_ > size_) {
        LOGGER_ERROR(
            "LRU (%zu) or TTL (%zu) size larger than overall size (%zu)",
            lfu_size_,
            ttl_size_,
            size_);
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
PredictiveLFUCache::insert(CacheAccess const &access)
{
    int nr_queues = 0;
    statistics_.insert(access.size_bytes());
    double const ttl_ms = access.ttl_ms;
    map_.emplace(access.key, CachePredictiveMetadata{access});
    auto &metadata = map_.at(access.key);
    auto [lo_t, hi_t, updated] =
        lifetime_thresholds_.at(1).get_updated_thresholds(access.timestamp_ms);
    if (!std::isinf(lo_t) && ttl_ms >= lo_t) {
        pred_tracker.record_store_lru();
        lfu_cache_.at(1).access(access.key);
        lfu_size_ += access.size_bytes();
        lfu_nr_obj_ += 1;
        metadata.set_lru();
        nr_queues += 1;
    }
    if (hi_t != 0.0 && ttl_ms <= hi_t) {
        pred_tracker.record_store_ttl();
        ttl_cache_.emplace(access.expiration_time_ms(), access.key);
        ttl_size_ += access.size_bytes();
        metadata.set_ttl();
        nr_queues += 1;
    }
    assert(nr_queues);
    size_ += access.size_bytes();
}

void
PredictiveLFUCache::update_remove_lfu(CacheAccess const &access,
                                      CachePredictiveMetadata &metadata,
                                      size_t const prev_frequency)
{
    assert(prev_frequency <= nr_lfu_buckets_);
    lfu_cache_.at(prev_frequency).remove(access.key);
    lfu_size_ -= metadata.size_;
    lfu_nr_obj_ -= 1;
    metadata.unset_lru();
}
void
PredictiveLFUCache::update_add_lfu(CacheAccess const &access,
                                   CachePredictiveMetadata &metadata,
                                   size_t const next_frequency)
{
    pred_tracker.record_store_lru();
    lfu_cache_.at(next_frequency).access(access.key);
    lfu_size_ += access.size_bytes();
    lfu_nr_obj_ += 1;
    metadata.set_lru();
}
void
PredictiveLFUCache::update_remove_ttl(CacheAccess const &access,
                                      CachePredictiveMetadata &metadata)
{
    remove_multimap_kv(ttl_cache_, metadata.expiration_time_ms_, access.key);
    ttl_size_ -= metadata.size_;
    metadata.unset_ttl();
}
void
PredictiveLFUCache::update_keep_ttl(CacheAccess const &access,
                                    CachePredictiveMetadata &metadata)
{
    assert(metadata.uses_ttl());
    pred_tracker.record_store_ttl();
    ttl_size_ += access.size_bytes() - metadata.size_;
}
void
PredictiveLFUCache::update_add_ttl(CacheAccess const &access,
                                   CachePredictiveMetadata &metadata)
{
    pred_tracker.record_store_ttl();
    ttl_size_ += access.size_bytes();
    ttl_cache_.emplace(metadata.expiration_time_ms_, access.key);
    metadata.set_ttl();
}

/// @brief  Process an access to an item in the cache.
void
PredictiveLFUCache::update(CacheAccess const &access,
                           CachePredictiveMetadata &metadata)
{
    size_t const prev_frequency = metadata.frequency_;
    size_t const next_frequency = metadata.frequency_ + 1;
    bool const maybe_lfu = next_frequency < lifetime_thresholds_.size();
    size_ += access.size_bytes() - metadata.size_;
    statistics_.update(metadata.size_, access.size_bytes());
    metadata.visit_without_ttl_refresh(access);
    double const ttl_ms = metadata.ttl_ms(access.timestamp_ms);
    auto [lo_t, hi_t, updated] =
        maybe_lfu ? lifetime_thresholds_.at(next_frequency)
                        .get_updated_thresholds(access.timestamp_ms)
                  : std::tuple<double, double, bool>{0.0, INFINITY, false};
    if (maybe_lfu && !std::isinf(lo_t) && ttl_ms >= lo_t) {
        if (metadata.uses_lru()) {
            update_remove_lfu(access, metadata, prev_frequency);
        }
        update_add_lfu(access, metadata, next_frequency);
    } else if (metadata.uses_lru()) {
        update_remove_lfu(access, metadata, prev_frequency);
    }
    if (hi_t != 0.0 && ttl_ms <= hi_t) {
        // Even if we don't re-insert into the TTL queue, we still
        // want to mark it as stored.
        if (metadata.uses_ttl()) {
            update_keep_ttl(access, metadata);
        } else {
            update_add_ttl(access, metadata);
        }
    } else if (metadata.uses_ttl()) {
        update_remove_ttl(access, metadata);
    }
}

void
PredictiveLFUCache::remove_lfu(uint64_t const victim_key,
                               CachePredictiveMetadata const &m,
                               CacheAccess const *const current_access,
                               EvictionCause const cause)
{
    lfu_cache_.at(m.frequency_).remove(victim_key);
    lfu_size_ -= m.size_;
    lfu_nr_obj_ -= 1;
    if (cause == EvictionCause::MainCapacity) {
        lifetime_thresholds_.at(m.frequency_)
            .register_cache_eviction(current_access->timestamp_ms -
                                         m.last_access_time_ms_,
                                     m.size_,
                                     current_access->timestamp_ms);
    }
}

/// @brief  Evict an object in the cache (either due to the eviction
///         policy or TTL expiration).
void
PredictiveLFUCache::remove(uint64_t const victim_key,
                           EvictionCause const cause,
                           CacheAccess const *const current_access)
{
    ok(true);
    CachePredictiveMetadata &m = map_.at(victim_key);
    uint64_t sz_bytes = m.size_;
    double exp_tm = m.expiration_time_ms_;

    // Update metadata tracking
    switch (cause) {
    case EvictionCause::MainCapacity:
        assert(current_access != NULL);
        statistics_.lru_evict(m.size_, m.ttl_ms(current_access->timestamp_ms));
        if (statistics_.current_time_ms_ <= exp_tm) {
            // Not yet expired.
            pred_tracker.update_correctly_evicted(sz_bytes);
        } else {
            pred_tracker.update_wrongly_evicted(sz_bytes);
        }
        break;
    case EvictionCause::ProactiveTTL:
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
    if (m.uses_lru()) {
        remove_lfu(victim_key, m, current_access, cause);
    }
    if (m.uses_ttl()) {
        if (cause == EvictionCause::VolatileTTL &&
            m.frequency_ <= nr_lfu_buckets_) {
            lifetime_thresholds_.at(m.frequency_)
                .register_cache_eviction(current_access->timestamp_ms -
                                             m.last_access_time_ms_,
                                         m.size_,
                                         current_access->timestamp_ms);
        }
        remove_multimap_kv(ttl_cache_, exp_tm, victim_key);
        ttl_size_ -= m.size_;
    }
    map_.erase(victim_key);
}

void
PredictiveLFUCache::evict_expired_objects(uint64_t const current_time_ms)
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
        remove(victim, EvictionCause::ProactiveTTL, nullptr);
    }
}

/// @return number of bytes evicted.
uint64_t
PredictiveLFUCache::evict_from_lfu(uint64_t const target_bytes,
                                   CacheAccess const &access)
{
    ok(true);
    uint64_t const ignored_key = access.key;
    uint64_t evicted_bytes = 0;
    std::vector<uint64_t> victims;
    for (auto const &[f, lru_cache_] : lfu_cache_) {
        for (auto const n : lru_cache_) {
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
    }
    // One cannot evict elements from the map one is iterating over.
    for (auto v : victims) {
        remove(v, EvictionCause::MainCapacity, &access);
    }
    return evicted_bytes;
}

uint64_t
PredictiveLFUCache::evict_smallest_ttl(uint64_t const target_bytes,
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
PredictiveLFUCache::ensure_enough_room(size_t const old_nbytes,
                                       CacheAccess const &access)
{
    size_t const new_nbytes = access.size_bytes();
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
    uint64_t const lru_evicted_b = evict_from_lfu(reqd_b, access);
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
PredictiveLFUCache::evict_expired_accessed_object(CacheAccess const &access)
{
    remove(access.key, EvictionCause::AccessExpired, &access);
}

void
PredictiveLFUCache::evict_too_big_accessed_object(CacheAccess const &access)
{
    remove(access.key, EvictionCause::NoRoom, &access);
}

bool
PredictiveLFUCache::is_expired(CacheAccess const &access,
                               CachePredictiveMetadata const &metadata)
{
    return object_is_expired(metadata.expiration_time_ms_, access.timestamp_ms);
}

void
PredictiveLFUCache::hit(CacheAccess const &access)
{
    auto &metadata = map_.at(access.key);
    if (!ensure_enough_room(metadata.size_, access)) {
        statistics_.skip(access.size_bytes());
        evict_too_big_accessed_object(access);
        if (DEBUG) {
            LOGGER_WARN("too big updated object");
        }
        return;
    }
    update(access, metadata);
}

bool
PredictiveLFUCache::miss(CacheAccess const &access)
{
    if (!ensure_enough_room(0, access)) {
        if (DEBUG) {
            LOGGER_WARN("not enough room to insert!");
        }
        statistics_.skip(access.size_bytes());
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
PredictiveLFUCache::PredictiveLFUCache(
    size_t const capacity,
    double const lower_ratio,
    double const upper_ratio,
    double const shards_sampling_ratio,
    std::map<std::string, std::string> kwargs,
    size_t const nr_lfu_buckets)
    : capacity_(capacity),
      oracle_(capacity, shards_sampling_ratio),
      kwargs_(kwargs),
      nr_lfu_buckets_(nr_lfu_buckets)

{
    for (size_t i = 0; i < nr_lfu_buckets; ++i) {
        lfu_cache_.emplace(i + 1, HashList{});
    }
    for (size_t i = 0; i < nr_lfu_buckets + 1; ++i) {
        // NOTE The 0th element is unused but is just for prettier
        // indexing because LFU starts at 1. Is this a terrible idea?
        // Yeah, probably.
        lifetime_thresholds_.push_back(
            LifeTimeThresholds{lower_ratio, upper_ratio});
    }
}

void
PredictiveLFUCache::start_simulation()
{
    statistics_.start_simulation();
    oracle_.start_simulation();
}

void
PredictiveLFUCache::end_simulation()
{
    statistics_.end_simulation();
    oracle_.end_simulation();
}

int
PredictiveLFUCache::access(CacheAccess const &access)
{
    ok(true);
    g_assert_cmpuint(size_, ==, statistics_.size_);
    statistics_.time(access.timestamp_ms);
    evict_expired_objects(access.timestamp_ms);
    oracle_.access(access);
    rm_policy_statistics_.access(access,
                                 lfu_nr_obj_,
                                 lfu_size_,
                                 ttl_cache_.size(),
                                 ttl_size_);
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
PredictiveLFUCache::size() const
{
    return size_;
}

size_t
PredictiveLFUCache::capacity() const
{
    return capacity_;
}

CachePredictiveMetadata const *
PredictiveLFUCache::get(uint64_t const key)
{
    if (map_.count(key)) {
        return &map_.at(key);
    }
    return nullptr;
}

void
PredictiveLFUCache::print()
{
    std::cout << "> PredictiveLFUCache(sz: " << size_ << ", cap: " << capacity_
              << ")\n";
    std::cout << "> \tLFU: ";
    for (auto const &[f, lru_cache_] : lfu_cache_) {
        std::cout << " /*" << f << "*/ ";
        for (auto n : lru_cache_) {
            std::cout << n->key << ", ";
        }
    }
    std::cout << "\n";
    std::cout << "> \tTTL: ";
    for (auto [tm, key] : ttl_cache_) {
        std::cout << key << "@" << tm << ", ";
    }
    std::cout << "\n";
}

PredictionTracker const &
PredictiveLFUCache::predictor() const
{
    return pred_tracker;
}

CacheStatistics const &
PredictiveLFUCache::statistics() const
{
    return statistics_;
}

std::string
PredictiveLFUCache::json(std::map<std::string, std::string> extras) const
{
    std::function<std::string(LifeTimeThresholds const &)> lambda =
        [](LifeTimeThresholds const &val) -> std::string { return val.json(); };
    auto [lo_t, hi_t] = lifetime_thresholds_.at(1).thresholds();
    auto [lo_r, hi_r] = lifetime_thresholds_.at(1).ratios();
    return map2str(std::vector<std::pair<std::string, std::string>>{
        {"Capacity [B]", format_memory_size(capacity_)},
        {"Lower Ratio", val2str(lo_r)},
        {"Upper Ratio", val2str(hi_r)},
        {"Statistics", statistics_.json()},
        {"Removal Policy Statistics", rm_policy_statistics_.json()},
        {"PredictionTracker", pred_tracker.json()},
        {"Oracle", oracle_.json()},
        {"Lifetime Thresholds", vec2str(lifetime_thresholds_, lambda)},
        {"Lower Threshold [ms]", val2str(format_time(lo_t))},
        {"Upper Threshold [ms]", val2str(format_time(hi_t))},
        {"Kwargs", map2str(kwargs_, true)},
        {"Extras", map2str(extras, false)},
    });
}

void
PredictiveLFUCache::print_json(std::ostream &ostrm,
                               std::map<std::string, std::string> extras) const
{
    ostrm << "> " << json(extras) << std::endl;
}
