/** @brief  Analyze the GET and SET requests in Kia's trace.
 */

#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_command.hpp"
#include "cpp_lib/cache_trace.hpp"
#include "cpp_lib/cache_trace_format.hpp"
#include "cpp_lib/format_measurement.hpp"
#include "cpp_lib/histogram.hpp"
#include "cpp_lib/progress_bar.hpp"
#include "trace/reader.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdbool.h>
#include <unordered_map>

using size_t = std::size_t;
using uint64_t = std::uint64_t;
using uint32_t = std::uint32_t;
using scount_t = std::uint16_t;
using tm_t = std::uint32_t;

static constexpr tm_t INVALID_TIME = UINT32_MAX;

static inline bool
valid_time(tm_t const t)
{
    return t != INVALID_TIME;
}

static inline std::string
prettify_number(uint64_t const num, uint64_t const den)
{
    return format_underscore(num) + " (" + format_percent((double)num / den) +
           ")";
}

struct AccessStatistics {
    void
    access(CacheAccess const &access)
    {
        if (access.command == CacheCommand::Get ||
            access.command == CacheCommand::Gets) {
            nr_read += 1;
            if (!valid_time(first_get_time_ms)) {
                first_get_time_ms = access.timestamp_ms;
            }
            if (!valid_time(first_set_time_ms)) {
                gets_before_first_set += 1;
            }
            gets_after_last_set += 1;
            latest_get_time_ms = access.timestamp_ms;
        } else if (access.command >= CacheCommand::Set &&
                   access.command <= CacheCommand::Decr) {
            nr_write += 1;
            if (!valid_time(first_set_time_ms)) {
                first_set_time_ms = access.timestamp_ms;
            }
            gets_after_last_set = 0;
            latest_set_time_ms = access.timestamp_ms;
            if (valid_time(current_ttl_ms) &&
                current_ttl_ms != access.time_to_live_ms()) {
                if (current_ttl_ms < access.time_to_live_ms()) {
                    ttl_increases += 1;
                } else if (current_ttl_ms < access.time_to_live_ms()) {
                    ttl_decreases += 1;
                }
            } else {
                ttl_remains += 1;
            }
            tm_t new_ttl = access.time_to_live_ms();
            current_ttl_ms = new_ttl;
            // The 'value_or' automatically sets it.
            min_ttl_ms = !valid_time(min_ttl_ms)
                             ? new_ttl
                             : std::min(min_ttl_ms, new_ttl);
            max_ttl_ms = !valid_time(max_ttl_ms)
                             ? new_ttl
                             : std::min(max_ttl_ms, new_ttl);
        } else {
            assert(0 && "unrecognized operation!");
        }
    }

    scount_t nr_read = 0;
    scount_t nr_write = 0;

    scount_t gets_before_first_set = 0;
    scount_t gets_after_last_set = 0;
    scount_t ttl_remains = 0;
    scount_t ttl_increases = 0;
    scount_t ttl_decreases = 0;

    tm_t first_set_time_ms = INVALID_TIME;
    tm_t first_get_time_ms = INVALID_TIME;
    tm_t latest_set_time_ms = INVALID_TIME;
    tm_t latest_get_time_ms = INVALID_TIME;
    tm_t current_ttl_ms = INVALID_TIME;
    tm_t min_ttl_ms = INVALID_TIME;
    tm_t max_ttl_ms = INVALID_TIME;
};

void
analyze_statistics_per_key(
    std::unordered_map<uint64_t, AccessStatistics> const &map,
    size_t const num_keys)
{
    uint64_t change_ttl = 0;
    uint64_t incr_ttl = 0;
    uint64_t decr_ttl = 0;
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
    Histogram nr_reads, nr_writes;
    Histogram max_ttl_per_key;
    Histogram min_ttl_per_key;
    for (auto [k, stats] : map) {
        nr_reads.update(stats.nr_read);
        nr_writes.update(stats.nr_write);
        if (valid_time(stats.min_ttl_ms)) {
            min_ttl_per_key.update(stats.min_ttl_ms);
        }
        if (valid_time(stats.max_ttl_ms)) {
            max_ttl_per_key.update(stats.max_ttl_ms);
        }

        // Count the keys where the TTL changes at least once.
        change_ttl += (stats.ttl_increases || stats.ttl_decreases);
        incr_ttl += stats.ttl_increases ? 1 : 0;
        decr_ttl += stats.ttl_decreases ? 1 : 0;
        if (valid_time(stats.first_set_time_ms) &&
            valid_time(stats.first_get_time_ms)) {
            assert(valid_time(stats.latest_set_time_ms));
            assert(valid_time(stats.latest_get_time_ms));

            if (valid_time(stats.first_get_time_ms) >
                    valid_time(stats.first_set_time_ms) &&
                valid_time(stats.latest_get_time_ms) <
                    valid_time(stats.latest_set_time_ms)) {
                set_get_set += 1;
            } else if (valid_time(stats.first_get_time_ms) <
                           valid_time(stats.first_set_time_ms) &&
                       valid_time(stats.latest_get_time_ms) >
                           valid_time(stats.latest_set_time_ms)) {
                get_set_get += 1;
            } else if (valid_time(stats.first_get_time_ms) <
                       valid_time(stats.first_set_time_ms)) {
                get_set += 1;
            } else if (valid_time(stats.latest_get_time_ms) >
                       valid_time(stats.latest_set_time_ms)) {
                set_get += 1;
            } else {
                same_time += 1;
            }
        } else if (!valid_time(stats.first_set_time_ms)) {
            get_only += 1;
        } else if (!valid_time(stats.first_get_time_ms)) {
            set_only += 1;
        } else {
            // A key without any GETs or SETs should not be in the map.
            assert(0);
        }
    }

    std::cout << "Number of keys: " << format_underscore(num_keys) << std::endl;
    std::cout << "Number of keys with multiple TTLs: "
              << prettify_number(change_ttl, num_keys) << std::endl;
    std::cout << "Number of keys with increasing TTLs: "
              << prettify_number(incr_ttl, num_keys) << std::endl;
    std::cout << "Number of keys with decreasing TTLs: "
              << prettify_number(decr_ttl, num_keys) << std::endl;
    std::cout << "Nr. Reads:" << std::endl;
    std::cout << nr_reads.csv();
    std::cout << "Nr. Writes:" << std::endl;
    std::cout << nr_writes.csv();
    std::cout << "Histogram of MIN TTLs: [ms] " << std::endl;
    std::cout << min_ttl_per_key.csv();
    std::cout << "Histogram of MAX TTLs [ms]: " << std::endl;
    std::cout << max_ttl_per_key.csv();
    std::cout << "GET of key only: " << prettify_number(get_only, num_keys)
              << std::endl;
    std::cout << "SET of key only: " << prettify_number(set_only, num_keys)
              << std::endl;
    std::cout << "First GET of key before first SET: "
              << prettify_number(get_set, num_keys) << std::endl;
    std::cout << "Last GET of key after last SET: "
              << prettify_number(set_get, num_keys) << std::endl;
    std::cout << "GET of key surrounded by SETs: "
              << prettify_number(set_get_set, num_keys) << std::endl;
    std::cout << "SET of key surrounded by GETs: "
              << prettify_number(get_set_get, num_keys) << std::endl;
    std::cout << "SET and GET at same time: "
              << prettify_number(same_time, num_keys) << std::endl;
    std::cout << "Sum (should equal #keys): "
              << prettify_number(get_only + set_only + get_set + set_get +
                                     set_get_set + get_set_get + same_time,
                                 num_keys)
              << std::endl;
}

void
analyze_statistics_per_access(
    std::unordered_map<uint64_t, AccessStatistics> const &map,
    size_t const num_accesses,
    size_t const cnt_sets)
{
    uint64_t gets_before_first_set = 0;
    uint64_t gets_after_last_set = 0;
    uint64_t ttl_changes = 0;
    uint64_t ttl_increase = 0;
    uint64_t ttl_decrease = 0;
    for (auto [k, stats] : map) {
        gets_before_first_set += stats.gets_before_first_set;
        gets_after_last_set += stats.gets_after_last_set;
        ttl_changes += stats.ttl_increases + stats.ttl_decreases;
        ttl_increase += stats.ttl_increases;
        ttl_decrease += stats.ttl_decreases;
    }
    std::cout << "Number of accesses: " << format_underscore(num_accesses)
              << std::endl;
    std::cout << "GET access before first SET: "
              << prettify_number(gets_before_first_set, num_accesses)
              << std::endl;
    std::cout << "GET access after first SET: "
              << prettify_number(num_accesses - gets_before_first_set,
                                 num_accesses)
              << std::endl;
    std::cout << "GET accesses after last SET: "
              << prettify_number(gets_after_last_set, num_accesses)
              << std::endl;
    std::cout << "GET accesses before last SET: "
              << prettify_number(num_accesses - gets_after_last_set,
                                 num_accesses)
              << std::endl;
    // We should compare this to the number of SET requests, not the
    // number of SET+GET requests, since they only change on SET requests.
    std::cout << "Accesses where TTL changes (compared to SET requests): "
              << prettify_number(ttl_changes, cnt_sets) << std::endl;
    std::cout << "Accesses where TTL increases (compared to TTL changes): "
              << prettify_number(ttl_increase, ttl_changes) << std::endl;
    std::cout << "Accesses where TTL decreases (compared to TTL changes): "
              << prettify_number(ttl_decrease, ttl_changes) << std::endl;
}

void
analyze_trace(char const *const trace_path,
              CacheTraceFormat const format,
              bool const show_progress,
              bool const verbose = false)
{
    uint64_t cnt_gets = 0, cnt_sets = 0;
    Histogram ttl_diff_hist;
    Histogram ttl_hist;

    std::unordered_map<uint64_t, AccessStatistics> map;
    CacheAccessTrace const trace(trace_path, format);
    ProgressBar pbar{trace.size(), show_progress};
    for (size_t i = 0; i < trace.size(); ++i) {
        pbar.tick();
        auto &x = trace.get(i);

        if (x.command == CacheCommand::Get) {
            cnt_gets += 1;
            map[x.key].access(x);
            continue;
        }

        ttl_hist.update(x.time_to_live_ms());
        // Analyze whether the TTL has changed before we update the
        // object with the new TTL.
        if (valid_time(map[x.key].current_ttl_ms)) {
            double const old_ttl = map.at(x.key).current_ttl_ms;
            double const new_ttl = x.ttl_ms.value_or(INFINITY);
            ttl_diff_hist.update((double)old_ttl - new_ttl);
            if (verbose && old_ttl != new_ttl) {
                std::cout << "TTL mismatch: " << old_ttl << " vs " << new_ttl
                          << std::endl;
            }
        }
        cnt_sets += 1;
        map[x.key].access(x);
    }

    std::cout << "# Trace Analysis for " << trace_path << " ("
              << get_trace_format_string(TRACE_FORMAT_KIA) << "'s format)"
              << std::endl;

    std::cout << "## Commands" << std::endl;
    std::cout << "Number of SETs: " << prettify_number(cnt_sets, trace.size())
              << std::endl;
    std::cout << "Number of GETs: " << prettify_number(cnt_gets, trace.size())
              << std::endl;

    std::cout << "## TTLs" << std::endl;
    std::cout << "TTL Histogram [ms]: " << std::endl;
    std::cout << ttl_hist.csv();
    std::cout << "Changes in TTLs Histogram [ms]: " << std::endl;
    std::cout << ttl_diff_hist.csv();
    std::cout << "---" << std::endl;
    analyze_statistics_per_key(map, map.size());
    std::cout << "---" << std::endl;
    analyze_statistics_per_access(map, trace.size(), cnt_sets);
}

bool
parse_bool(char const *str)
{
    if (std::strcmp(str, "true") == 0) {
        return true;
    }
    if (std::strcmp(str, "false") == 0) {
        return true;
    }
    assert(0 && "unrecognized bool parameter");
}

int
main(int argc, char *argv[])
{
    if (argc != 3 && argc != 4) {
        std::cout << "Usage: " << argv[0]
                  << " <trace-path> <format> [<show_progress>=true]"
                  << std::endl;
        return EXIT_FAILURE;
    }
    char const *const trace_path = argv[1];
    CacheTraceFormat format = CacheTraceFormat__parse(argv[2]);
    bool const show_progress = (argc == 4) ? parse_bool(argv[3]) : true;
    assert(CacheTraceFormat__valid(format));
    analyze_trace(trace_path, format, show_progress);
    return 0;
}
