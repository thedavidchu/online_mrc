#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "random/uniform_random.h"

/**
 * C is a run-time constant randomly chosen within [0 .. A] that can be
 * varied without altering performance. The same C value, per field
 * (C_LAST, C_ID, and OL_I_ID), must be used by all emulated terminals.
 * constexpr, but let's not bother C++11.
 */
static uint32_t
get_c(uint32_t A)
{
    // yes, I'm lazy. but this satisfies the spec.
    const uint64_t kCSeed = 0x734b00c6d7d3bbdaULL;
    return (uint32_t)(kCSeed % (A + 1));
}

bool
uniform_random__init(struct UniformRandom *me, const uint64_t seed)
{
    if (me == NULL) {
        return false;
    }
    me->seed_ = seed;
    return true;
}

uint32_t
uniform_random__next_uint32(struct UniformRandom *me)
{
    me->seed_ = me->seed_ * 0xD04C3175 + 0x53DA9022;
    return (uint32_t)((me->seed_ >> 32) ^ (me->seed_ & 0xFFFFFFFF));
}

uint64_t
uniform_random__next_uint64(struct UniformRandom *me)
{
    return (((uint64_t)uniform_random__next_uint32(me)) << 32) | uniform_random__next_uint32(me);
}

uint32_t
uniform_random__within(struct UniformRandom *me, uint32_t from, uint32_t to)
{
    if (from == to) {
        return from;
    }
    return from + (uniform_random__next_uint32(me) % (to - from + 1));
}

uint32_t
uniform_random__within_except(struct UniformRandom *me, uint32_t from, uint32_t to, uint32_t except)
{
    while (true) {
        uint32_t val = uniform_random__within(me, from, to);
        if (val != except) {
            return val;
        }
    }
}

uint32_t
uniform_random__non_uniform_within(struct UniformRandom *me, uint32_t A, uint32_t from, uint32_t to)
{
    uint32_t C = get_c(A);
    return (((uniform_random__within(me, 0, A) | uniform_random__within(me, from, to)) + C) %
            (to - from + 1)) +
           from;
}
