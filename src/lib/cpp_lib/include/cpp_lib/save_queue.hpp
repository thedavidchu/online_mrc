/** @brief  At a specific trigger time, save to a file. */
#pragma once

#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_predictive_metadata.hpp"
#include "cpp_struct/hash_list.hpp"
#include <string>
#include <unordered_map>

class SaveQueue {
public:
    SaveQueue(double const trigger_time_ms, std::string const output_path);

    bool
    save(CacheAccess const &access,
         HashList const &queue,
         std::unordered_map<uint64_t, CachePredictiveMetadata> const &map);

    bool
    done() const;

private:
    bool done_ = false;
    double const trigger_time_ms_;
    std::string const output_path_;
};
