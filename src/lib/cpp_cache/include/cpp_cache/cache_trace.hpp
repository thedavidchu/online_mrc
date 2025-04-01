#pragma once
#include "cpp_cache/cache_access.hpp"
#include "cpp_cache/cache_trace_format.hpp"

#include "io/io.h"

#include <cassert>
#include <cstddef>
#include <string>

class CacheAccessTrace {
public:
    CacheAccessTrace(std::string const &fname, CacheTraceFormat const format);

    ~CacheAccessTrace();

    size_t
    size() const;

    CacheAccess const
    get(size_t const i) const;

private:
    size_t const bytes_per_obj_ = 0;
    CacheTraceFormat const format_ = CacheTraceFormat::Invalid;

    struct MemoryMap mm_ = {};
    size_t length_ = 0;
};
