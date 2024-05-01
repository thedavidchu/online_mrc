#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "random/uniform_random.h"
#include "random/zipfian_random.h"

static double
zeta(struct ZipfianRandom *me, uint64_t n)
{
    double sum = 0;
    for (uint64_t i = 0; i < n; i++) {
        sum += 1 / pow(i + 1, me->theta_);
    }
    return sum;
}

bool
ZipfianRandom__init(struct ZipfianRandom *me, uint64_t items, double theta, uint64_t urnd_seed)
{
    if (me == NULL) {
        return false;
    }
    me->max_ = items - 1;
    me->theta_ = theta;
    me->zetan_ = zeta(me, items);
    me->alpha_ = 1.0 / (1.0 - me->theta_);
    me->eta_ = (1 - pow(2.0 / (double)items, 1 - me->theta_)) / (1 - zeta(me, 2) / me->zetan_);
    UniformRandom__init(&me->urnd_, urnd_seed);
    return true;
}

uint64_t
ZipfianRandom__next(struct ZipfianRandom *me)
{
    double u = UniformRandom__within(&me->urnd_, 0, (uint32_t)me->max_) / (double)me->max_;
    double uz = u * me->zetan_;
    if (uz < 1.0) {
        return 0;
    }

    if (uz < 1.0 + pow(0.5, me->theta_)) {
        return 1;
    }

    uint64_t ret = (uint64_t)((double)me->max_ * pow(me->eta_ * u - me->eta_ + 1, me->alpha_));
    return ret;
}

void
ZipfianRandom__destroy(struct ZipfianRandom *me)
{
    *me = (struct ZipfianRandom){0};
}