#pragma once
#include <stdbool.h>

#include "run/runner_arguments.h"
#include "trace/reader.h"

/// @brief  Run the oracle in a memory-efficient (but slow) manner.
/// @note   It may be advisable to run this only if the following
///         exceeds the physical memory of the system:
///         1. Trace
///             -- compute this by checking the file size.
///         2. Hash table
///             -- should be able to hold roughly twice the number of
///                elements as in the WSS.
///         3. Splay tree
///             -- two pointers, value, order-statistic per unique item.
///         4. Histogram
///             -- may be up to twice as many values as the WSS.
///         5. MRC
///         where WSS is the (expected?) working set size.
bool
run_oracle(char const *const restrict trace_path,
           enum TraceFormat const format,
           struct RunnerArguments const *const args);

bool
run_oracle_with_ttl(char const *const restrict trace_path,
                    enum TraceFormat const format,
                    struct RunnerArguments const *const args);
