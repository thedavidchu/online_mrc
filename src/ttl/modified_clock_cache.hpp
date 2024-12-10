#include <cassert>
#include <cstdint>
#include <map>
#include <unordered_map>

#include "cache_statistics.hpp"
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
run_ttl_modified_clock_cache(char const *const trace_path,
                             enum TraceFormat const format,
                             uint64_t const capacity)
{
    LOGGER_TRACE("running '%s()", __func__);
    std::size_t const bytes_per_trace_item = get_bytes_per_trace_item(format);

    // This initializes everything to the default, i.e. 0.
    struct MemoryMap mm = {};
    std::size_t num_entries = 0;
    std::unordered_map<uint64_t, struct TTLForModifiedClock> map;
    std::multimap<std::uint64_t, std::uint64_t> expiration_queue;
    CacheStatistics statistics;
    uint64_t ttl_s = 1 << 30;

    if (trace_path == NULL || bytes_per_trace_item == 0) {
        LOGGER_ERROR("invalid input", format);
        goto cleanup_error;
    }

    // Memory map the input trace file
    if (!MemoryMap__init(&mm, trace_path, "rb")) {
        LOGGER_ERROR("failed to mmap '%s'", trace_path);
        goto cleanup_error;
    }
    num_entries = mm.num_bytes / bytes_per_trace_item;

    // Run trace
    for (size_t i = 0; i < num_entries; ++i) {
        if (i % 1000000 == 0) {
            LOGGER_TRACE("Finished %zu / %zu", i, num_entries);
        }
        struct FullTraceItemResult r = construct_full_trace_item(
            &((uint8_t *)mm.buffer)[i * bytes_per_trace_item],
            format);
        assert(r.valid);

        // Skip PUT requests.
        if (r.item.command == 1) {
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

        if (map.count(r.item.key)) {
            struct TTLForModifiedClock &s = map[r.item.key];
            s.last_access_time_ms = r.item.timestamp_ms;
            s.ttl_s = ttl_s;

            statistics.hit();
        } else {
            map[r.item.key] = {
                r.item.timestamp_ms,
                r.item.timestamp_ms,
                ttl_s,
            };
            uint64_t eviction_time_ms =
                saturation_add(r.item.timestamp_ms,
                               saturation_multiply(1000, ttl_s));
            expiration_queue.emplace(eviction_time_ms, r.item.key);

            statistics.miss();
        }
    }

    assert(statistics.total_accesses_ < num_entries);
    statistics.print("LRU by TTLs", capacity);

    MemoryMap__destroy(&mm);
    return statistics.miss_rate();
cleanup_error:
    MemoryMap__destroy(&mm);
    return -1.0;
}
