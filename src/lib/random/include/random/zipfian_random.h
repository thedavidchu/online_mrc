/*
 * Copyright (c) 2014-2015, Hewlett-Packard Development Company, LP.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details. You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * HP designates this particular file as subject to the "Classpath" exception
 * as provided by HP in the LICENSE.txt file that accompanied this code.
 *
 * MODIFIED: Adapted for compatibility with the C99 language by David Chu
 */
#pragma once

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "random/uniform_random.h"

/**
 * @brief A simple zipfian generator based off of YCSB's Java implementation.
 * The major user is YCSB. 0 <= theta < 1, higher means more skewed.
 * Generates a random number between 0 and max_.
 * @ingroup ASSORTED
 */
struct ZipfianRandom {
    struct UniformRandom urnd_;
    uint64_t max_;
    uint64_t base_;
    double theta_;
    double zetan_;
    double alpha_;
    double eta_;
};

/// NOTE    This function is O(N) where N=#items.
bool
zipfian_random__init(struct ZipfianRandom *me,
                     uint64_t items,
                     double theta,
                     uint64_t urnd_seed);

uint64_t
zipfian_random__next(struct ZipfianRandom *me);
