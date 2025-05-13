#include "cpp_lib/cache_trace.hpp"
#include "cpp_lib/cache_access.hpp"

#include "cpp_lib/cache_trace_format.hpp"
#include "cpp_lib/progress_bar.hpp"
#include "io/io.h"
#include "logger/logger.h"
#include "math/is_nth_iter.h"

#include <cassert>
#include <cstddef>
#include <pthread.h>
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

/// @note   My understanding is that the std::string is copied on assignment.
///         This would mean that the path_ is a copy of the fname param.
CacheAccessTrace::CacheAccessTrace(std::string const &fname,
                                   CacheTraceFormat const format,
                                   size_t const nthreads)
    : path_(fname),
      format_(format),
      nthreads_(nthreads),
      mm_(mm_init(fname.c_str())),
      bytes_per_obj_(CacheTraceFormat__bytes_per_entry(format)),
      length_(mm_.num_bytes / bytes_per_obj_)
{
    if (nthreads_ == 0) {
        LOGGER_ERROR("invalid number of threads %zu", nthreads_);
        exit(1);
    }
    if (bytes_per_obj_ == 0) {
        LOGGER_ERROR("invalid format %s",
                     CacheTraceFormat__string(format).c_str());
        exit(1);
    }
    int err = 0;
    if ((err = pthread_barrier_init(&barrier_, nullptr, nthreads))) {
        exit(1);
    }
}

CacheAccessTrace::~CacheAccessTrace()
{
    MemoryMap__kdestroy(&mm_);
    pthread_barrier_destroy(&barrier_);
}

std::string const &
CacheAccessTrace::path() const
{
    return path_;
}

CacheTraceFormat
CacheAccessTrace::format() const
{
    return format_;
}

size_t
CacheAccessTrace::nthreads() const
{
    return nthreads_;
}

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

CacheAccess const
CacheAccessTrace::get_wait(size_t const i)
{
    if (is_nth_iter(i, 1024)) {
        pthread_barrier_wait(&barrier_);
    }
    return CacheAccess{&((uint8_t *)mm_.buffer)[i * bytes_per_obj_], format_};
}

CacheAccess const
CacheAccessTrace::front() const
{
    if (size() == 0) {
        return CacheAccess{0, 0, 0, 0};
    }
    return get(0);
}

CacheAccess const
CacheAccessTrace::back() const
{
    if (size() == 0) {
        return CacheAccess{0, 0, 0, 0};
    }
    return get(size() - 1);
}
