/** @brief  Represent a cache access. */
#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "io/io.h"
#include "logger/logger.h"
#include "math/saturation_arithmetic.h"
#include "trace/reader.h"
#include "trace/trace.h"

enum class CacheAccessCommand : uint8_t {
    get,
    set,
};

struct CacheAccess {
    std::uint64_t const timestamp_ms;
    CacheAccessCommand const command;
    std::uint64_t const key;
    std::uint32_t const size_bytes;
    std::optional<std::uint64_t> const ttl_ms;

    /// @brief  Initialize a CacheAccess object from a FullTraceItem.
    CacheAccess(struct FullTraceItem const *const item)
        : timestamp_ms(item->timestamp_ms),
          command((CacheAccessCommand)item->command),
          key(item->key),
          size_bytes(item->size),
          // A TTL of 0 in Kia's traces implies no TTL. I assume it's
          // the same in Sari's, but I don't know.
          ttl_ms(item->ttl_s == 0
                     ? std::nullopt
                     : std::optional(saturation_multiply(1000, item->ttl_s)))
    {
    }

    CacheAccess(std::uint64_t const timestamp_ms, std::uint64_t const key)
        : timestamp_ms(timestamp_ms),
          command(CacheAccessCommand::get),
          key(key),
          size_bytes(1),
          ttl_ms(std::nullopt)
    {
    }

    CacheAccess(std::uint64_t const timestamp_ms,
                std::uint64_t const key,
                std::uint64_t size_bytes,
                std::optional<std::uint64_t> ttl_ms)
        : timestamp_ms(timestamp_ms),
          command(CacheAccessCommand::get),
          key(key),
          size_bytes(size_bytes),
          ttl_ms(ttl_ms)
    {
    }
};

class CacheAccessTrace {
public:
    CacheAccessTrace(std::string const &fname, enum TraceFormat const format)
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

    ~CacheAccessTrace() { MemoryMap__destroy(&mm_); }

    size_t
    size() const
    {
        return length_;
    }

    CacheAccess const
    get(size_t const i) const
    {
        struct FullTraceItemResult r = construct_full_trace_item(
            &((uint8_t *)mm_.buffer)[i * bytes_per_obj_],
            format_);
        assert(r.valid);
        return CacheAccess{&r.item};
    }

private:
    size_t const bytes_per_obj_ = 0;
    enum TraceFormat const format_ = TRACE_FORMAT_INVALID;

    struct MemoryMap mm_ = {};
    size_t length_ = 0;
};
