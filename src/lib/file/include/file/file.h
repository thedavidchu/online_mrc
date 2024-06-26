#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

bool
write_buffer(char const *const file_name,
             void const *const buffer,
             size_t const nmemb,
             size_t size);

/// @note   I'm not sure whether I should make a new file for this function,
///         since "io" is a bit misleading.
bool
file_exists(char const *const file_name);
