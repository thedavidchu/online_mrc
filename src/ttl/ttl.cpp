#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "cache_statistics.hpp"
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
run_ttl_lru_cache(char const *const trace_path,
                  enum TraceFormat const format,
                  uint64_t const capacity)
{
    LOGGER_TRACE("running 'run_ttl_lru_cache()");
    size_t const bytes_per_trace_item = get_bytes_per_trace_item(format);

    // This initializes everything to the default, i.e. 0.
    struct MemoryMap mm = {};
    size_t num_entries = 0;
    std::unordered_map<uint64_t, struct TTLForLRU> map;
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
            struct TTLForLRU &s = map[victim_key];
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
            struct TTLForLRU &s = map[r.item.key];
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

static double
run_ttl_fifo_cache(char const *const trace_path,
                   enum TraceFormat const format,
                   uint64_t const capacity)
{
    LOGGER_TRACE("running 'run_ttl_fifo_cache()");
    size_t const bytes_per_trace_item = get_bytes_per_trace_item(format);

    // This initializes everything to the default, i.e. 0.
    struct MemoryMap mm = {};
    size_t num_entries = 0;
    std::unordered_map<uint64_t, bool> map;
    std::multimap<std::uint64_t, std::uint64_t> expiration_queue;
    CacheStatistics statistics;
    std::uint64_t logical_time = 0;
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
            std::uint64_t victim_key = x->second;
            expiration_queue.erase(x);
            std::size_t i = map.erase(victim_key);
            assert(i == 1);
            assert(map.size() + 1 == capacity);
        }

        if (map.count(r.item.key)) {
            map[r.item.key] = true;
            statistics.hit();
        } else {
            map[r.item.key] = false;
            uint64_t eviction_time_ms =
                saturation_add(logical_time, saturation_multiply(1000, ttl_s));
            expiration_queue.emplace(eviction_time_ms, r.item.key);
            statistics.miss();
        }

        ++logical_time;
    }

    assert(statistics.total_accesses_ < num_entries);
    statistics.print("LRU by TTLs", capacity);

    MemoryMap__destroy(&mm);
    return statistics.miss_rate();
cleanup_error:
    MemoryMap__destroy(&mm);
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
    bool run_clock = false, run_fifo = false, run_ttl_lru = false,
         run_ttl_fifo = false;
    std::cout << "Hello, World!" << std::endl;
    for (int i = 0; i < argc && *argv != NULL; ++i, ++argv) {
        std::string arg = std::string(*argv);
        std::cout << "Arg " << i << ": '" << arg << "'" << std::endl;
        if (arg == "fifo") {
            run_fifo = true;
        } else if (arg == "clock") {
            run_clock = true;
        } else if (arg == "ttl-lru") {
            run_ttl_lru = true;
        } else if (arg == "ttl-fifo") {
            run_ttl_fifo = true;
        }
    }

    std::vector<std::size_t> sizes = {
        1000,   2000,   3000,   4000,   5000,   6000,   7000,   8000,
        9000,   10000,  20000,  30000,  40000,  50000,  60000,  70000,
        80000,  90000,  100000, 200000, 300000, 310000, 320000, 330000,
        340000, 350000, 360000, 370000, 380000, 390000, 400000,
    };
    std::vector<std::pair<std::uint64_t, double>> ttl_lru_mrc;
    std::vector<std::pair<std::uint64_t, double>> ttl_fifo_mrc;
    std::vector<std::pair<std::uint64_t, double>> clock_mrc;
    std::vector<std::pair<std::uint64_t, double>> fifo_mrc;

    if (run_ttl_lru) {
        for (auto sz : sizes) {
            double mr = run_ttl_lru_cache(
                "/home/david/projects/online_mrc/data/src2.bin",
                TRACE_FORMAT_KIA,
                sz);
            assert(mr != -1.0);
            ttl_lru_mrc.push_back({sz, mr});
        }
    }
    if (run_ttl_fifo) {
        for (auto sz : sizes) {
            double mr = run_ttl_fifo_cache(
                "/home/david/projects/online_mrc/data/src2.bin",
                TRACE_FORMAT_KIA,
                sz);
            assert(mr != -1.0);
            ttl_fifo_mrc.push_back({sz, mr});
        }
    }
    if (run_clock) {
        clock_mrc =
            generate_mrc<ClockCache>(
                "/home/david/projects/online_mrc/data/src2.bin",
                TRACE_FORMAT_KIA,
                sizes)
                .value_or(std::vector<std::pair<std::uint64_t, double>>());
    }
    if (run_fifo) {
        fifo_mrc =
            generate_mrc<FIFOCache>(
                "/home/david/projects/online_mrc/data/src2.bin",
                TRACE_FORMAT_KIA,
                sizes)
                .value_or(std::vector<std::pair<std::uint64_t, double>>());
    }

    std::cout << ClockCache::name << std::endl;
    for (auto [sz, mr] : clock_mrc) {
        std::cout << sz << "," << mr << std::endl;
    }
    std::cout << FIFOCache::name << std::endl;
    for (auto [sz, mr] : fifo_mrc) {
        std::cout << sz << "," << mr << std::endl;
    }

    std::cout << "TTL-LRU" << std::endl;
    for (auto [sz, mr] : ttl_lru_mrc) {
        std::cout << sz << "," << mr << std::endl;
    }
    std::cout << "TTL-FIFO" << std::endl;
    for (auto [sz, mr] : ttl_fifo_mrc) {
        std::cout << sz << "," << mr << std::endl;
    }
    return 0;
}
