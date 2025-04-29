#include <cassert>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_statistics.hpp"
#include "cpp_lib/cache_trace.hpp"
#include "cpp_lib/cache_trace_format.hpp"
#include "io/io.h"
#include "logger/logger.h"
#include "math/saturation_arithmetic.h"
#include "trace/reader.h"
#include "trace/trace.h"

struct TTLForModifiedClock {
    std::uint64_t insert_time_ms;
    std::uint64_t last_access_time_ms;
    std::uint64_t ttl_s;
};

static inline double
run_ttl_modified_clock_cache(CacheAccessTrace const &trace,
                             uint64_t const capacity)
{
    LOGGER_TRACE("running '%s()", __func__);

    std::unordered_map<uint64_t, struct TTLForModifiedClock> map;
    std::multimap<std::uint64_t, std::uint64_t> expiration_queue;
    CacheStatistics statistics;
    uint64_t ttl_s = 1 << 30;

    for (size_t i = 0; i < trace.size(); ++i) {
        if (i % 1000000 == 0) {
            LOGGER_TRACE("Finished %zu / %zu", i, trace.size());
        }
        CacheAccess access = trace.get(i);
        if (!access.is_read()) {
            continue;
        }
        if (capacity == 0) {
            statistics.deprecated_miss();
            continue;
        }
        assert(map.size() == expiration_queue.size());
        if (map.size() >= capacity) {
            auto const x = expiration_queue.begin();
            std::uint64_t eviction_time_ms = x->first;
            std::uint64_t victim_key = x->second;
            expiration_queue.erase(x);
            // If unaccessed since last sweep, remove!
            struct TTLForModifiedClock &s = map[victim_key];
            std::uint64_t new_eviction_time_ms =
                saturation_add(s.last_access_time_ms,
                               saturation_multiply(1000, s.ttl_s));
            if (eviction_time_ms == new_eviction_time_ms) {
                std::size_t i = map.erase(victim_key);
                assert(i == 1);
            } else {
                auto x =
                    expiration_queue.emplace(new_eviction_time_ms, victim_key);
                assert(x->first == new_eviction_time_ms &&
                       x->second == victim_key);
            }
        }

        if (map.count(access.key)) {
            struct TTLForModifiedClock &s = map[access.key];
            s.last_access_time_ms = access.timestamp_ms;
            s.ttl_s = ttl_s;

            statistics.deprecated_hit();
        } else {
            map[access.key] = {
                access.timestamp_ms,
                access.timestamp_ms,
                ttl_s,
            };
            uint64_t eviction_time_ms =
                saturation_add(access.timestamp_ms,
                               saturation_multiply(1000, ttl_s));
            expiration_queue.emplace(eviction_time_ms, access.key);

            statistics.deprecated_miss();
        }
    }

    assert(statistics.total_ops() < trace.size());
    statistics.print("LRU by TTLs", capacity);

    return statistics.miss_rate();
}

static inline std::optional<std::map<std::uint64_t, double>>
generate_modified_clock_mrc(char const *const trace_path,
                            CacheTraceFormat const format,
                            std::vector<std::size_t> const &capacities)
{
    std::map<std::size_t, double> mrc = {};
    CacheAccessTrace trace = CacheAccessTrace{trace_path, format};
    for (auto sz : capacities) {
        double mr = run_ttl_modified_clock_cache(trace, sz);
        assert(mr != -1.0);
        mrc[sz] = mr;
    }
    return mrc;
}
