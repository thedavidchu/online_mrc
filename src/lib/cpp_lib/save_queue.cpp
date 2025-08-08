
#include "cpp_lib/save_queue.hpp"
#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_predictive_metadata.hpp"
#include "cpp_lib/remaining_lifetime.hpp"
#include "cpp_lib/util.hpp"
#include "cpp_struct/hash_list.hpp"
#include "logger/logger.h"
#include <fstream>
#include <ios>
#include <map>
#include <string>
#include <unordered_map>

SaveQueue::SaveQueue(double const trigger_time_ms,
                     std::string const output_path)
    : trigger_time_ms_{trigger_time_ms},
      output_path_{output_path}
{
}

bool
SaveQueue::save(
    CacheAccess const &access,
    HashList const &queue,
    std::unordered_map<uint64_t, CachePredictiveMetadata> const &map)
{
    if (done_ || access.timestamp_ms < trigger_time_ms_) {
        return false;
    }
    RemainingLifetime rl{queue, map, access.timestamp_ms, 1000};

    std::ofstream of{output_path_, std::ios::out | std::ios::trunc};
    auto s = map2str(std::map<std::string, std::string>{
        {"Extras",
         map2str(std::map<std::string, std::string>{
             {"remaining_lifetime", rl.json()}})}});
    of.write(s.c_str(), s.size());
    of.close();
    LOGGER_INFO("printed to %s", output_path_.c_str());
    done_ = true;
    return true;
}

bool
SaveQueue::done() const
{
    return done_;
}
