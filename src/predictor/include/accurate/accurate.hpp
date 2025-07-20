#pragma once
#include "cpp_lib/cache_access.hpp"
#include <unordered_map>

class Accurate {
public:
    void
    start_simulation();

    void
    end_simulation();

    int
    access(CacheAccess const &access);

    std::string
    json(std::unordered_map<std::string, std::string> extras = {}) const;
};
