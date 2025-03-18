/** @brief  Analyze the SET requests in Kia's trace.
 *
 *  @todo   1. How many SET before GET [only]
 *  @todo   1. How many SET before and after GET
 *  @todo   1. How many SET after GET [only]
 *  @todo   1. No SET
 */

#include "cpp_cache/cache_access.hpp"
#include "cpp_cache/cache_trace.hpp"
#include "cpp_cache/histogram.hpp"
#include "cpp_cache/progress_bar.hpp"
#include "trace/reader.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <unordered_map>

using uint64_t = std::uint64_t;
using size_t = std::size_t;

static inline uint64_t
absdiff(uint64_t const a, uint64_t const b)
{
    if (a > b) {
        return a - b;
    } else {
        return b - a;
    }
}

struct AccessStatistics {
    void
    access(CacheAccess const &access)
    {
        if (access.command == CacheAccessCommand::get) {
            if (!first_get_time_ms.has_value()) {
                first_get_time_ms = access.timestamp_ms;
            }
            if (!first_set_time_ms.has_value()) {
                gets_before_first_set += 1;
            }
            gets_after_last_set += 1;
            latest_get_time_ms = access.timestamp_ms;
        } else if (access.command == CacheAccessCommand::set) {
            if (!first_set_time_ms.has_value()) {
                first_set_time_ms = access.timestamp_ms;
            }
            gets_after_last_set = 0;
            latest_set_time_ms = access.timestamp_ms;
            current_ttl_ms = access.ttl_ms.value_or(UINT64_MAX);
        } else {
            assert(0 && "impossible!");
        }
    }

    uint64_t gets_before_first_set = 0;
    uint64_t gets_after_last_set = 0;

    std::optional<uint64_t> first_set_time_ms = std::nullopt;
    std::optional<uint64_t> first_get_time_ms = std::nullopt;
    std::optional<uint64_t> latest_set_time_ms = std::nullopt;
    std::optional<uint64_t> latest_get_time_ms = std::nullopt;
    std::optional<uint64_t> current_ttl_ms = std::nullopt;
};

void
analyze_statistics_per_key(
    std::unordered_map<uint64_t, AccessStatistics> const &map,
    size_t const num_keys)
{
    uint64_t get_only = 0;
    uint64_t set_only = 0;
    // Number of keys where first GET happens before first SET
    uint64_t get_set = 0;
    // Number of keys where last GET happens after last SET
    uint64_t set_get = 0;
    // Number of keys where first set happens before the first GET and
    // the last SET happens after the last GET.
    uint64_t set_get_set = 0;
    // Number of keys where first set happens after the first GET and
    // the last SET happens before the last GET.
    uint64_t get_set_get = 0;
    // The first GET and first SET happen at the same time; the last GET
    // and the last SET happen at the same time. It is possible that the
    // first and last are the same or different times.
    uint64_t same_time = 0;
    for (auto [k, stats] : map) {
        if (stats.first_set_time_ms.has_value() &&
            stats.first_get_time_ms.has_value()) {
            assert(stats.latest_set_time_ms.has_value());
            assert(stats.latest_get_time_ms.has_value());

            if (stats.first_get_time_ms.value() >
                    stats.first_set_time_ms.value() &&
                stats.latest_get_time_ms.value() <
                    stats.latest_set_time_ms.value()) {
                set_get_set += 1;
            } else if (stats.first_get_time_ms.value() <
                           stats.first_set_time_ms.value() &&
                       stats.latest_get_time_ms.value() >
                           stats.latest_set_time_ms.value()) {
                get_set_get += 1;
            } else if (stats.first_get_time_ms.value() <
                       stats.first_set_time_ms.value()) {
                get_set += 1;
            } else if (stats.latest_get_time_ms.value() >
                       stats.latest_set_time_ms.value()) {
                set_get += 1;
            } else {
                same_time += 1;
            }
        } else if (!stats.first_set_time_ms.has_value()) {
            get_only += 1;
        } else if (!stats.first_get_time_ms.has_value()) {
            set_only += 1;
        } else {
            // A key without any GETs or SETs should not be in the map.
            assert(0);
        }
    }

    std::cout << "GET of key only: " << get_only << "/" << num_keys
              << std::endl;
    std::cout << "SET of key only: " << set_only << "/" << num_keys
              << std::endl;
    std::cout << "First GET of key before first SET: " << get_set << "/"
              << num_keys << std::endl;
    std::cout << "Last GET of key after last SET: " << set_get << "/"
              << num_keys << std::endl;
    std::cout << "GET of key surrounded by SETs: " << set_get_set << "/"
              << num_keys << std::endl;
    std::cout << "SET of key surrounded by GETs: " << get_set_get << "/"
              << num_keys << std::endl;
    std::cout << "SET and GET at same time: " << same_time << "/" << num_keys
              << std::endl;
    std::cout << "Sum (should equal #keys): "
              << get_only + set_only + get_set + set_get + set_get_set +
                     get_set_get + same_time
              << std::endl;
}

void
analyze_statistics_per_access(
    std::unordered_map<uint64_t, AccessStatistics> const &map,
    size_t const num_accesses)
{
    uint64_t gets_before_first_set = 0;
    uint64_t gets_after_last_set = 0;
    for (auto [k, stats] : map) {
        gets_before_first_set += stats.gets_before_first_set;
        gets_after_last_set += stats.gets_after_last_set;
    }
    std::cout << "GET access before first SET: " << gets_before_first_set << "/"
              << num_accesses << std::endl;
    std::cout << "GET accesses after last SET: " << gets_after_last_set << "/"
              << num_accesses << std::endl;
}

void
analyze_trace(char const *const trace_path,
              enum TraceFormat const format,
              bool const verbose = false)
{
    Histogram ttl_diff_hist;

    std::unordered_map<uint64_t, AccessStatistics> map;
    CacheAccessTrace const trace(trace_path, format);
    ProgressBar pbar(trace.size());
    for (size_t i = 0; i < trace.size(); ++i) {
        pbar.tick();
        auto &x = trace.get(i);

        if (!x.ttl_ms.has_value()) {
            map[x.key].access(x);
            continue;
        }

        // Analyze whether the TTL has changed before we update the
        // object with the new TTL.
        if (map[x.key].current_ttl_ms.has_value()) {
            uint64_t old_ttl = map.at(x.key).current_ttl_ms.value();
            uint64_t new_ttl = x.ttl_ms.value();
            uint64_t diff = absdiff(old_ttl, new_ttl);
            ttl_diff_hist.update(diff);
            if (verbose && diff != 0) {
                std::cout << "TTL mismatch: " << old_ttl << " vs " << new_ttl
                          << std::endl;
            }
        }
        map[x.key].access(x);
    }

    std::cout << "Min TTL diff [ms]: " << ttl_diff_hist.percentile(0.0)
              << std::endl;
    std::cout << "Q1 TTL diff [ms]: " << ttl_diff_hist.percentile(0.25)
              << std::endl;
    std::cout << "Mean TTL diff [ms]: " << ttl_diff_hist.mean() << std::endl;
    std::cout << "Mode TTL diff [ms]: " << ttl_diff_hist.mode() << std::endl;
    std::cout << "Median TTL diff [ms]: " << ttl_diff_hist.percentile(0.5)
              << std::endl;
    std::cout << "Q3 TTL diff [ms]: " << ttl_diff_hist.percentile(0.75)
              << std::endl;
    std::cout << "Max TTL diff [ms]: " << ttl_diff_hist.percentile(1.0)
              << std::endl;
    std::cout << "---" << std::endl;
    analyze_statistics_per_key(map, map.size());
    std::cout << "---" << std::endl;
    analyze_statistics_per_access(map, trace.size());
}

int
main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <trace-path> <format>"
                  << std::endl;
        return EXIT_FAILURE;
    }
    char const *const trace_path = argv[1];
    enum TraceFormat format = parse_trace_format_string(argv[2]);
    assert(format == TRACE_FORMAT_KIA || format == TRACE_FORMAT_SARI);
    analyze_trace(trace_path, format);
    return 0;
}
