#include "cpp_lib/cache_trace.hpp"
#include "cpp_lib/cache_trace_format.hpp"
#include "cpp_lib/progress_bar.hpp"
#include "cpp_lib/util.hpp"
#include "lib/lfu_ttl_cache.hpp"
#include "shards/fixed_rate_shards_sampler.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

struct CommandLineArguments {
private:
    static void
    print_usage(std::string exe)
    {
        std::cout << "> Usage: " << exe
                  << " <input-path> <format Sari|Kia> <policy LRU|LFU> "
                     "<capacities \"1KiB 2KiB\"> <shards_ratio (0.0,1.0]>"
                  << std::endl;
    }

public:
    CommandLineArguments(int const argc, char const *const argv[])
    {
        if (argc != 1 + 5) {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }

        input_path = argv[1];
        trace_format = CacheTraceFormat__parse(argv[2]);
        assert(CacheTraceFormat__valid(trace_format));
        policy = argv[3];
        std::unordered_set<std::string> ok_policies{"LRU", "LFU"};
        assert(ok_policies.contains(policy));
        cache_capacities = parse_capacities(argv[4]);
        shards_ratio = atof(argv[5]);
        assert(0 < shards_ratio && shards_ratio <= 1.0);
    }

    std::string input_path;
    CacheTraceFormat trace_format;
    std::string policy;
    std::vector<uint64_t> cache_capacities;
    double shards_ratio;
    // I hard-code this as false because I have everything print at the end so
    // it doesn't make sense to include progress bars if they're not printed in
    // real time.
    bool const show_progress = false;
};

bool
run_single_lfu_cache(std::promise<std::string> promise,
                     uint64_t id,
                     CacheAccessTrace &trace,
                     uint64_t const capacity_bytes,
                     double const shards_ratio,
                     bool const show_progress)
{
    LFU_TTL_Cache cache{capacity_bytes};
    FixedRateShardsSampler sampler = {};
    if (!FixedRateShardsSampler__init(&sampler, shards_ratio, true)) {
        return false;
    }
    std::stringstream ss, shards_ss;
    LOGGER_TIMING("starting test_trace(trace: %s, nominal cap: %zu, sampled "
                  "cap: %zu, shards: %f)",
                  trace.path().c_str(),
                  capacity_bytes,
                  (size_t)(capacity_bytes * shards_ratio),
                  shards_ratio);
    ProgressBar pbar{trace.size(), show_progress && id == 0};
    cache.start_simulation();
    for (size_t i = 0; i < trace.size(); ++i) {
        pbar.tick();
        auto const &access = trace.get_wait(i);
        if (!FixedRateShardsSampler__sample(&sampler, access.key)) {
            continue;
        }
        if (access.is_read()) {
            cache.access(access);
        }
    }
    cache.end_simulation();
    LOGGER_TIMING("finished test_trace(trace: %s, cap: %zu, shards: %f)",
                  trace.path().c_str(),
                  capacity_bytes,
                  shards_ratio);

    FixedRateShardsSampler__json(shards_ss, sampler, false);
    ss << cache.json({{"SHARDS", shards_ss.str()}});
    promise.set_value(ss.str());
    FixedRateShardsSampler__destroy(&sampler);

    return true;
}

void
run_lfu(CommandLineArguments const &args)
{
    CacheAccessTrace trace{args.input_path,
                           args.trace_format,
                           args.cache_capacities.size()};
    std::vector<std::thread> workers;
    std::vector<std::future<std::string>> futs;

    int id = 0;
    for (auto c : args.cache_capacities) {
        std::promise<std::string> promise;
        futs.push_back(promise.get_future());
        workers.emplace_back(run_single_lfu_cache,
                             std::move(promise),
                             id++,
                             std::ref(trace),
                             c,
                             args.shards_ratio,
                             false);
    }
    for (auto &w : workers) {
        w.join();
    }
    int i = 0;
    for (auto &r : futs) {
        std::cout << "Run: " << args.input_path << " "
                  << CacheTraceFormat__string(args.trace_format) << " "
                  << args.cache_capacities[i++] << " " << std::endl;
        std::cout << "> " << r.get() << std::endl;
    }
}

int
main(int argc, char *argv[])
{
    CommandLineArguments args{argc, argv};

    if (args.policy == "LRU") {
        assert(0 && "unimplemented");
    } else if (args.policy == "LFU") {
        run_lfu(args);
    }

    return 0;
}
