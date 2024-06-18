#include <stdbool.h>

#include "arrays/array_size.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "random/uniform_random.h"
#include "sampler/phase_sampler.h"
#include "test/mytester.h"

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

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_phase_sampler());
    return 0;
}
