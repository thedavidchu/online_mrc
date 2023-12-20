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
