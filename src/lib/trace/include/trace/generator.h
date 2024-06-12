#pragma once

#include <stddef.h>
#include <stdint.h>

struct Trace
generate_zipfian_trace(const uint64_t length,
                       const uint64_t max_num_unique_entries,
                       const double skew,
                       const uint64_t seed);

struct Trace
generate_step_trace(const uint64_t length,
                    const uint64_t max_num_unique_entries);

struct Trace
generate_two_step_trace(const uint64_t length,
                        const uint64_t max_num_unique_entries);

struct Trace
generate_two_distribution_trace(const size_t length,
                                const size_t max_num_unique_entries);
