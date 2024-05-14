#pragma once

#include <bits/stdint-uintn.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "random/uniform_random.h"
#include "types/key_type.h"

struct ReservoirSamplingAlgorithmR {
    size_t index;
    size_t reservoir_size;
    KeyType *reservoir;
    struct UniformRandom urng;
};

struct ReservoirSample {
    bool evict;
    KeyType victim;
};

bool
ReservoirSamplingAlgorithmR__init(struct ReservoirSamplingAlgorithmR *const me,
                                  size_t const reservoir_size,
                                  uint64_t const seed);

struct ReservoirSample
ReservoirSamplingAlgorithmR__sample(
    struct ReservoirSamplingAlgorithmR *const me,
    KeyType const key);

void
ReservoirSamplingAlgorithmR__destroy(
    struct ReservoirSamplingAlgorithmR *const me);
