#include "cpp_cache/cache_trace.hpp"
#include "cpp_cache/cache_access.hpp"

#include "cpp_cache/cache_trace_format.hpp"
#include "io/io.h"
#include "logger/logger.h"
#include "math/is_nth_iter.h"

#include <cassert>
#include <cstddef>
#include <string>
#include <sys/mman.h>

static inline struct MemoryMap
mm_init(char const *const fname)
{
    struct MemoryMap mm = {};
    if (!MemoryMap__init(&mm, fname, "rb")) {
        LOGGER_ERROR("failed to mmap '%s'", fname);
        exit(1);
    }
    return mm;
}

CacheAccessTrace::CacheAccessTrace(std::string const &fname,
                                   CacheTraceFormat const format)
    : bytes_per_obj_(CacheTraceFormat__bytes_per_entry(format)),
      format_(format),
      mm_(mm_init(fname.c_str())),
      length_(mm_.num_bytes / bytes_per_obj_)
{
    if (bytes_per_obj_ == 0) {
        LOGGER_ERROR("invalid format %s",
                     CacheTraceFormat__string(format).c_str());
        exit(1);
    }
}

CacheAccessTrace::~CacheAccessTrace() { MemoryMap__kdestroy(&mm_); }

size_t
CacheAccessTrace::size() const
{
    return length_;
}

CacheAccess const
CacheAccessTrace::get(size_t const i) const
{
    return CacheAccess{&((uint8_t *)mm_.buffer)[i * bytes_per_obj_], format_};
}
