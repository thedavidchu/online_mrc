#pragma once

#include "trace/trace.h"

enum TraceFormat {
    TRACE_FORMAT_INVALID,
    /// So the data format is basically each access occupies 25 bytes in this
    /// order:
    /// 8 bytes -> u64 -> timestamp (unix timestamp)
    /// 1 byte -> u8 -> command (0 is get, 1 is set)
    /// 8 bytes -> u64 -> key
    /// 4 bytes -> u32 -> object size
    /// 4 bytes -> u32 -> ttl (zero means no ttl)
    /// Everything is formatted in little endian
    TRACE_FORMAT_KIA,
    /// Our access traces are binary formatted using the following format, and
    /// sorted by timestamp.
    /// | Property  | Type                           |
    /// | --------- | ------------------------------ |
    /// | Timestamp | Time in Seconds (uint32)       |
    /// | Key       | uint64                         |
    /// | Size      | uint32                         |
    /// | Eviction Time       | uint32 (TTL + Timestamp) |
    /// Each access thus requires 20 bytes.
    TRACE_FORMAT_SARI,
};

static char const *const TRACE_FORMAT_STRINGS[] = {"INVALID", "Kia", "Sari"};

/// @brief  Read the traces formatted by Kia and Sari.
struct Trace
read_trace(char const *const restrict file_name, enum TraceFormat format);
