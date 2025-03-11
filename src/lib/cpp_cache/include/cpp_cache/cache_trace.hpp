#pragma once
#include "cpp_cache/cache_access.hpp"

#include "io/io.h"
#include "logger/logger.h"
#include "trace/reader.h"

#include <cassert>
#include <cstddef>
#include <string>

class CacheAccessTrace {
public:
    CacheAccessTrace(std::string const &fname, enum TraceFormat const format);

    ~CacheAccessTrace();

    size_t
    size() const;

    CacheAccess const
    get(size_t const i) const;

private:
    size_t const bytes_per_obj_ = 0;
    enum TraceFormat const format_ = TRACE_FORMAT_INVALID;

    struct MemoryMap mm_ = {};
    size_t length_ = 0;
};
