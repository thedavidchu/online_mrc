/// NOTE    Unfortunately, the math_dep needs to be listed as a dependency in
///         Meson executables/libraries. You cannot do this for just includes.
///         This means that if you get weird errors complaining about this, you
///         included this directory without adding the math library as a
///         dependency.
/// TODO    Maybe if I refactored this into an actual C library, then I could
///         shield the math dependency from the user.
#pragma once

#include <float.h>
#include <math.h>
#include <stdbool.h>

#if false
inline bool
doubles_are_equal(double x, double y)
{
    return fabs(x - y) < DBL_EPSILON;
}
#else
#define doubles_are_equal(x, y) (fabs(((x)) - ((y))) < DBL_EPSILON)
#endif
