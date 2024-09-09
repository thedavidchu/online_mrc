/** @brief  This is a thin wrapper around a plain float64 array.
 *          It simply makes appending and saving multiple elements easier.
 *  @details    The format of the saved file is as follows:
 *              * 8 bytes: number of float64's per item, SIZE
 *              * NUMBER * SIZE * 8 bytes: the array of items, numbering NUMBER.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "array/binary64_array.h"

struct Statistics {
    // The number of fp64's per item in the statistics array.
    size_t b64_per_item;
    struct Binary64Array stats;
};

bool
Statistics__init(struct Statistics *const me, size_t const f64_per_item);

void
Statistics__destroy(struct Statistics *const me);

/// @brief  Append a 64-bit value to the array.
bool
Statistics__append_binary64(struct Statistics *const me,
                            void const *const data);

bool
Statistics__append_float64(struct Statistics *const me,
                           double const *const data);

bool
Statistics__append_uint64(struct Statistics *const me,
                          uint64_t const *const data);

bool
Statistics__save(struct Statistics const *const me, char const *const path);
