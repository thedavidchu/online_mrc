#pragma once

/// @brief  A ceiling divide for positive integers. This is a factored version
///         of ((n + d - 1) / d), thus eliminating the possibility of integer
///         addition overflow.
/// NOTE    The double ((brackets)) around the parameters is for the linter to
///         format the numerator as a variable rather than as a cast.
/// Source:
/// https://stackoverflow.com/questions/2745074/fast-ceiling-of-an-integer-division-in-c-c
#define POSITIVE_CEILING_DIVIDE(numerator, denominator)                        \
    (1 + (((numerator)) - 1) / ((denominator)))
