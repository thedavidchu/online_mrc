#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include "random/uniform_random.h"
#include "types/key_type.h"

#include "sampling/reservoir_sampling.h"

bool
ReservoirSamplingAlgorithmR__init(struct ReservoirSamplingAlgorithmR *const me,
                                  size_t const reservoir_size,
                                  uint64_t const seed)
{
    me->index = 0;
    me->reservoir_size = reservoir_size;
    me->reservoir = calloc(reservoir_size, sizeof(*me->reservoir));
    return UniformRandom__init(&me->urng, seed);
}

struct ReservoirSample
ReservoirSamplingAlgorithmR__sample(
    struct ReservoirSamplingAlgorithmR *const me,
    KeyType const key)
{
    if (me->index < me->reservoir_size) {
        me->reservoir[me->index] = key;
        ++me->index;
        return (struct ReservoirSample){.evict = false, .victim = 0};
    }

    uint64_t next = UniformRandom__next_uint64(&me->urng);
    uint64_t replacement_idx = next % me->index;
    if (replacement_idx < me->reservoir_size) {
        KeyType old_key = me->reservoir[replacement_idx];
        me->reservoir[replacement_idx] = key;
        ++me->index;
        return (struct ReservoirSample){.evict = true, .victim = old_key};
    }
    ++me->index;
    return (struct ReservoirSample){.evict = false, .victim = 0};
}

void
ReservoirSamplingAlgorithmR__destroy(
    struct ReservoirSamplingAlgorithmR *const me)
{
    if (me == NULL)
        return;
    UniformRandom__destroy(&me->urng);
    free(me->reservoir);
    *me = (struct ReservoirSamplingAlgorithmR){0};
}
