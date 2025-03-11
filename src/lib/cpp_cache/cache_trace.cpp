#include "cpp_cache/cache_trace.hpp"
#include "cpp_cache/cache_access.hpp"

#include "io/io.h"
#include "logger/logger.h"
#include "trace/reader.h"

#include <cassert>
#include <cstddef>
#include <string>

CacheAccessTrace::CacheAccessTrace(std::string const &fname,
                                   enum TraceFormat const format)
    : bytes_per_obj_(get_bytes_per_trace_item(format)),
      format_(format)
{
    if (bytes_per_obj_ == 0) {
        LOGGER_ERROR("invalid format %s", get_trace_format_string(format));
        exit(1);
    }
    // Memory map the input trace file
    if (!MemoryMap__init(&mm_, fname.c_str(), "rb")) {
        LOGGER_ERROR("failed to mmap '%s'", fname.c_str());
        exit(1);
    }
    length_ = mm_.num_bytes / bytes_per_obj_;
}

CacheAccessTrace::~CacheAccessTrace() { MemoryMap__destroy(&mm_); }

size_t
CacheAccessTrace::size() const
{
    return length_;
}

CacheAccess const
CacheAccessTrace::get(size_t const i) const
{
    struct FullTraceItemResult r =
        construct_full_trace_item(&((uint8_t *)mm_.buffer)[i * bytes_per_obj_],
                                  format_);
    assert(r.valid);
    return CacheAccess{&r.item};
}
