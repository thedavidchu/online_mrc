#include <stdbool.h>

#include "arrays/array_size.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "random/uniform_random.h"
#include "sampler/phase_sampler.h"
#include "test/mytester.h"
#include "unused/mark_unused.h"

////////////////////////////////////////////////////////////////////////////////
/// GLOBAL RANDOM NUMBER GENERATOR
////////////////////////////////////////////////////////////////////////////////
struct UniformRandom global_urng = {.seed_ = 42};

static uint64_t
rand_get(void)
{
    return UniformRandom__within(&global_urng, 1, 1 << 12);
}

static void
rand_reset(void)
{
    global_urng.seed_ = 42;
}

////////////////////////////////////////////////////////////////////////////////
/// BASIC PHASE SAMPLER TEST
////////////////////////////////////////////////////////////////////////////////
static struct Histogram
init_random_histogram(uint64_t const seed,
                      uint64_t const num_bins,
                      uint64_t const bin_size)
{
    assert(num_bins >= 1 && bin_size >= 1);

    struct UniformRandom urng = {0};
    bool r = UniformRandom__init(&urng, seed);
    assert(r);
    uint64_t *histogram = calloc(num_bins, sizeof(*histogram));
    assert(histogram != NULL);

    for (size_t i = 0; i < num_bins; ++i) {
        histogram[i] = UniformRandom__next_uint64(&urng);
    }

    struct Histogram hist = (struct Histogram){
        .histogram = histogram,
        .num_bins = num_bins,
        .bin_size = bin_size,
        .false_infinity = UniformRandom__within(&urng, 1, 100),
        .infinity = UniformRandom__within(&urng, 1, 100),
        .running_sum = 0,
    };

    hist.running_sum = Histogram__calculate_running_sum(&hist);
    return hist;
}

static bool
test_phase_sampler(void)
{
    struct PhaseSampler me = {0};
    PhaseSampler__init(&me);

    // I use a random num_bins and bin_size to ensure that it truly is
    // saving the num_bins and bin_size properly.
    rand_reset();
    for (size_t i = 0; i < 5; ++i) {
        struct Histogram hist =
            init_random_histogram(i, rand_get(), rand_get());
        PhaseSampler__change_histogram(&me, &hist);
        Histogram__destroy(&hist);
    }

    rand_reset();
    for (size_t i = 0; i < 5; ++i) {
        struct Histogram oracle =
            init_random_histogram(i, rand_get(), rand_get());
        struct Histogram hist = {0};

        LOGGER_TRACE("reading from '%s'",
                     g_ptr_array_index(me.saved_histograms, i));
        Histogram__init_from_file(&hist,
                                  g_ptr_array_index(me.saved_histograms, i));
        g_assert_true(Histogram__exactly_equal(&oracle, &hist));
        Histogram__destroy(&oracle);
        Histogram__destroy(&hist);
    }

    PhaseSampler__destroy(&me);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
/// TEST PHASE SAMPLER MRC Generation
////////////////////////////////////////////////////////////////////////////////
static struct Histogram
init_specific_histogram(uint64_t const seed,
                        uint64_t const num_bins,
                        uint64_t const bin_size)
{
    UNUSED(seed);
    assert(num_bins >= 1 && bin_size >= 1);

    uint64_t *histogram = calloc(num_bins, sizeof(*histogram));
    assert(histogram != NULL);
    for (size_t i = 0; i < num_bins; ++i) {
        histogram[i] = i;
    }

    struct Histogram hist = (struct Histogram){
        .histogram = histogram,
        .num_bins = num_bins,
        .bin_size = bin_size,
        .false_infinity = 100,
        .infinity = 100,
        .running_sum = 0,
    };

    hist.running_sum = Histogram__calculate_running_sum(&hist);
    return hist;
}

static bool
test_phase_sampler_mrc_generation(void)
{
    struct PhaseSampler me = {0};
    PhaseSampler__init(&me);

    // I use a random num_bins and bin_size to ensure that it truly is
    // saving the num_bins and bin_size properly.
    rand_reset();
    uint64_t const num_hist_bins = rand_get();
    uint64_t const bin_size = rand_get();

    for (size_t i = 0; i < 5; ++i) {
        struct Histogram hist =
            init_specific_histogram(i, num_hist_bins, bin_size);
        PhaseSampler__change_histogram(&me, &hist);
        Histogram__destroy(&hist);
    }

    struct MissRateCurve mrc = {0};
    g_assert_true(PhaseSampler__create_mrc(&me, &mrc, num_hist_bins, bin_size));

    // Right now, the 'init_specific_histogram' function generates all
    // identical histograms, so the average should be the same as any
    // generated member.
    struct Histogram oracle_hist =
        init_specific_histogram(0, num_hist_bins, bin_size);
    struct MissRateCurve oracle_mrc = {0};
    g_assert_true(
        MissRateCurve__init_from_histogram(&oracle_mrc, &oracle_hist));
    g_assert_true(MissRateCurve__all_close(&mrc, &oracle_mrc, DBL_EPSILON));

    // Destroy all the objects in no particular order
    PhaseSampler__destroy(&me);
    MissRateCurve__destroy(&mrc);
    Histogram__destroy(&oracle_hist);
    MissRateCurve__destroy(&oracle_mrc);
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_phase_sampler());
    ASSERT_FUNCTION_RETURNS_TRUE(test_phase_sampler_mrc_generation());
    return 0;
}
