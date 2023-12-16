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

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief A very simple and deterministic random generator that is more aligned with standard
 * benchmark such as TPC-C.
 * @ingroup ASSORTED
 * @details
 * Actually this is exactly from TPC-C spec.
 */
struct UniformRandom {
    uint64_t seed_;
};

bool
uniform_random__init(struct UniformRandom *me, const uint64_t seed);

uint32_t
uniform_random__next_uint32(struct UniformRandom *me);

uint64_t
uniform_random__next_uint64(struct UniformRandom *me);

/**
 * In TPCC terminology, from=x, to=y.
 * NOTE both from and to are _inclusive_.
 */
uint32_t
uniform_random__within(struct UniformRandom *me, uint32_t from, uint32_t to);

/**
 * Same as uniform_within() except it avoids the "except" value.
 * Make sure from!=to.
 */
uint32_t
uniform_random__within_except(struct UniformRandom *me,
                              uint32_t from,
                              uint32_t to,
                              uint32_t except);

/**
 * @brief Non-Uniform random (NURand) in TPCC spec (see Sec 2.1.6).
 * @details
 * In TPCC terminology, from=x, to=y.
 *  NURand(A, x, y) = (((random(0, A) | random(x, y)) + C) % (y - x + 1)) + x
 */
uint32_t
uniform_random__non_uniform_within(struct UniformRandom *me,
                                   uint32_t A,
                                   uint32_t from,
                                   uint32_t to);
