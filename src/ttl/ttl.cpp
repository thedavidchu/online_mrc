#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "clock_cache.hpp"
#include "fifo_cache.hpp"
#include "io/io.h"
#include "logger/logger.h"
#include "math/saturation_arithmetic.h"
#include "priority_queue/heap.h"
#include "trace/reader.h"
#include "trace/trace.h"

struct TTLForLRU {
    uint64_t insert_time_ms;
    uint64_t last_access_time_ms;
    uint64_t ttl_s;
};

static double
run_ttl_lru(char const *const trace_path,
            enum TraceFormat const format,
            uint64_t const capacity)
{
    LOGGER_TRACE("running 'run_ttl_lru()");
    size_t const bytes_per_trace_item = get_bytes_per_trace_item(format);

    // This initializes everything to the default, i.e. 0.
    struct MemoryMap mm = {};
    size_t num_entries = 0;
    std::unordered_map<uint64_t, struct TTLForLRU> map;
    struct Heap min_heap = {};
    uint64_t ttl_s = 1 << 30;

    size_t hits = 0;
    size_t misses = 0;
    size_t total_accesses = 0;

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

    if (!Heap__init_min_heap(&min_heap, 1 << 20)) {
        LOGGER_ERROR("failed to initialize min heap");
        goto cleanup_error;
    }

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

        if (map.size() >= capacity) {
            // Evict
            while (true) {
                uint64_t key = 0;
                uint64_t min_eviction_time_ms = Heap__get_top_key(&min_heap);
                bool r = Heap__remove(&min_heap, min_eviction_time_ms, &key);
                assert(r);
                // If unaccessed since last sweep, remove!
                struct TTLForLRU &s = map[key];
                uint64_t evict_tm =
                    saturation_add(s.last_access_time_ms,
                                   saturation_multiply(1000, s.ttl_s));
                if (min_eviction_time_ms == evict_tm) {
                    size_t i = map.erase(key);
                    assert(i == 1);
                    break;
                } else {
                    bool r = Heap__insert(&min_heap, evict_tm, key);
                    assert(r);
                }
            }
        }

        if (map.count(r.item.key)) {
            struct TTLForLRU &s = map[r.item.key];
            s.last_access_time_ms = r.item.timestamp_ms;
            s.ttl_s = ttl_s;

            ++hits;
        } else {
            map[r.item.key] = {
                r.item.timestamp_ms,
                r.item.timestamp_ms,
                ttl_s,
            };
            uint64_t evict_tm =
                saturation_add(r.item.timestamp_ms,
                               saturation_multiply(1000, ttl_s));
            Heap__insert(&min_heap, evict_tm, r.item.key);

            ++misses;
        }
        ++total_accesses;
    }

    assert(total_accesses < num_entries);
    LOGGER_ERROR("Capacity: %zu | Hits: %zu (~%f%%), Misses: %zu (=%f%%), "
                 "Total Accesses: %zu",
                 capacity,
                 hits,
                 (double)100 * hits / total_accesses,
                 misses,
                 (double)100 * misses / total_accesses,
                 total_accesses);

    MemoryMap__destroy(&mm);
    Heap__destroy(&min_heap);
    return (double)misses / total_accesses;
cleanup_error:
    MemoryMap__destroy(&mm);
    Heap__destroy(&min_heap);
    return -1.0;
}

template <typename T>
static double
run_cache(struct MemoryMap const *const mm,
          enum TraceFormat format,
          uint64_t const capacity)
{
    LOGGER_TRACE("running '%s' algorithm for size %zu", T::name, capacity);
    size_t num_entries = 0;
    size_t bytes_per_trace_item = 0;
    T cache(capacity);

    if (mm == NULL) {
        LOGGER_ERROR("invalid input");
        return -1.0;
    }
    // NOTE I let this helper function handle the format checks for me
    //      so that all logic dealing with parsing trace formats is
    //      contained within this library.
    bytes_per_trace_item = get_bytes_per_trace_item(format);
    if (bytes_per_trace_item == 0) {
        LOGGER_ERROR("invalid trace format");
        return -1.0;
    }
    num_entries = mm->num_bytes / bytes_per_trace_item;
    for (size_t i = 0; i < num_entries; ++i) {
        if (i % 1000000 == 0) {
            LOGGER_TRACE("Finished %zu / %zu", i, num_entries);
        }
        struct FullTraceItemResult r = construct_full_trace_item(
            &((uint8_t *)mm->buffer)[i * bytes_per_trace_item],
            format);
        assert(r.valid);

        // Skip PUT requests.
        if (r.item.command == 1) {
            continue;
        }

        cache.access_item(r.item.key);
    }
    assert(cache.statistics_.total_accesses_ < num_entries);
    cache.statistics_.print(T::name, capacity);
    return cache.statistics_.miss_rate();
}

template <typename T>
static std::optional<std::vector<std::pair<std::uint64_t, double>>>
generate_mrc(char const *const trace_path,
             enum TraceFormat const format,
             std::vector<std::size_t> const &capacities)
{
    // This initializes everything to the default, i.e. 0.
    struct MemoryMap mm = {};
    std::vector<std::pair<std::size_t, double>> mrc = {};

    if (trace_path == NULL) {
        LOGGER_ERROR("invalid input path", format);
        goto cleanup_error;
    }

    // Memory map the input trace file
    if (!MemoryMap__init(&mm, trace_path, "rb")) {
        LOGGER_ERROR("failed to mmap '%s'", trace_path);
        goto cleanup_error;
    }
    for (auto cap : capacities) {
        double mr = run_cache<T>(&mm, format, cap);
        if (mr == -1.0) {
            LOGGER_ERROR("error in '%s' algorithm (N.B. name may be mangled)",
                         typeid(T).name());
            return std::nullopt;
        };
        mrc.push_back({cap, mr});
    }
    return std::make_optional(mrc);
cleanup_error:
    MemoryMap__destroy(&mm);
    return std::nullopt;
}

int
main(int argc, char *argv[])
{
    std::cout << "Hello, World!" << std::endl;
    for (int i = 0; i < argc && *argv != NULL; ++i, ++argv) {
        std::cout << "Arg " << i << ": '" << *argv << "'" << std::endl;
    }

    std::vector<std::size_t> sizes = {
        1000,   2000,   3000,   4000,   5000,   6000,   7000,   8000,
        9000,   10000,  20000,  30000,  40000,  50000,  60000,  70000,
        80000,  90000,  100000, 200000, 300000, 310000, 320000, 330000,
        340000, 350000, 360000, 370000, 380000, 390000, 400000,
    };
    std::vector<std::pair<std::uint64_t, double>> ttl_lru_mrc = {};
    std::vector<std::pair<std::uint64_t, double>> ttl_fifo_mrc = {};

    std::vector<std::pair<std::uint64_t, double>> clock_mrc =
        generate_mrc<ClockCache>(
            "/home/david/projects/online_mrc/data/src2.bin",
            TRACE_FORMAT_KIA,
            sizes)
            .value_or(std::vector<std::pair<std::uint64_t, double>>());
    std::vector<std::pair<std::uint64_t, double>> fifo_mrc =
        generate_mrc<FIFOCache>("/home/david/projects/online_mrc/data/src2.bin",
                                TRACE_FORMAT_KIA,
                                sizes)
            .value_or(std::vector<std::pair<std::uint64_t, double>>());

    std::cout << ClockCache::name << std::endl;
    for (auto [sz, mr] : clock_mrc) {
        std::cout << sz << "," << mr << std::endl;
    }
    std::cout << FIFOCache::name << std::endl;
    for (auto [sz, mr] : fifo_mrc) {
        std::cout << sz << "," << mr << std::endl;
    }
    return 0;
}
