#pragma once
#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_trace_format.hpp"

#include "io/io.h"

#include <cassert>
#include <cstddef>
#include <pthread.h>
#include <string>

class CacheAccessTrace {
public:
    CacheAccessTrace(std::string const &fname,
                     CacheTraceFormat const format,
                     size_t const nthreads = 1);

    ~CacheAccessTrace();

    std::string const &
    path() const;

    CacheTraceFormat
    format() const;

    size_t
    nthreads() const;

    size_t
    size() const;

    CacheAccess const
    get(size_t const i) const;

    /// @brief  Get the thing and potentially wait for a barrier.
    CacheAccess const
    get_wait(size_t const i);

    CacheAccess const
    front() const;

    CacheAccess const
    back() const;

private:
    static constexpr size_t SYNC_SIZE = 1024;

    // Initialization data
    std::string const path_ = "";
    CacheTraceFormat const format_ = CacheTraceFormat::Invalid;
    size_t const nthreads_ = 1;

    // Internal data
    struct MemoryMap const mm_ = {};
    size_t const bytes_per_obj_ = 0;
    size_t const length_ = 0;

    // Synchronization
    // NOTE I use the pthread barrier because it supports a dynamic
    //      number of threads rather than the std::barrier whose number
    //      of threads needs to be known at compile-time AFAIK.
    pthread_barrier_t barrier_ = {};
};
