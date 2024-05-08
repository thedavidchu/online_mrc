#pragma once

#define UNUSED(x) ((void)((x)))

/// @brief  Mark a variable as potentially unused (e.g. in release builds).
/// @example    Assertions are optimized out in release builds.
///             ```c
///             bool r = function();
///             assert(r);          /* Optimized out in release builds*/
///             MAYBE_UNUSED(r);
///             ```
/// Source:
/// https://stackoverflow.com/questions/777261/avoiding-unused-variables-warnings-when-using-assert-in-a-release-build
#define MAYBE_UNUSED(x) ((void)((x)))
