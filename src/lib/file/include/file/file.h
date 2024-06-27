#pragma once

#include <stdbool.h>
#include <stddef.h>

bool
write_buffer(char const *const file_name,
             void const *const buffer,
             size_t const nmemb,
             size_t size);

bool
file_exists(char const *const file_name);

/// @note   The caller is the owner of the heap-allocated absolute path!
///         i.e. the caller must free it!
/// @note   This isn't efficient in the slightest. We call a few GLib
///         library functions, which call the memory allocator.
/// @note   This doesn't expand "~/" paths yet... this was the whole
///         purpose of this function in the first place LOL!
char *
get_absolute_path(char const *const relative_path);
