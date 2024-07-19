#pragma once
#include <stdbool.h>
#include <stddef.h>

static char const *const BOOLEAN_STRINGS[2] = {"false", "true"};

static inline char const *
maybe_string(char const *const str)
{
    return str == NULL ? "(null)" : str;
}

static inline char const *
bool_to_string(bool x)
{
    return x ? BOOLEAN_STRINGS[1] : BOOLEAN_STRINGS[0];
}
