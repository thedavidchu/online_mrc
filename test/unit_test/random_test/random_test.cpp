#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <iostream>

// HACK This is a complete hack just to get the C++ linker to work. AHHH!
extern "C" {
// TODO(dchu)   Test the Uniform Random function directly
// I don't test the uniform random function directly... yet.
#include "random/uniform_random.h"
#include "random/zipfian_random.h"
}

#include "random_test/uniform_random.hpp"
#include "random_test/zipfian_random.hpp"

#include "test/mytester.h"

uint64_t randomly_generated_bounds[100] = {};
double randomly_generated_thetas[100] = {};
// NOTE Generated with the following Python script:
//      import random; [random.randint(0, 1 << 20) for _ in range(100)]
uint64_t randomly_generated_seeds[100] = {
    141660, 480415, 620145, 329492, 645058, 688290,  142414,  565032, 269066, 542702,
    935407, 940243, 741758, 7487,   929452, 1024453, 649212,  596986, 488264, 341134,
    325642, 780511, 619697, 60228,  252594, 667931,  506263,  179048, 439014, 38977,
    407685, 734321, 961367, 842433, 875855, 61421,   1016821, 469277, 102209, 161102,
    97616,  425410, 174331, 252233, 25582,  575849,  1030875, 523705, 874388, 288983,
    932377, 214718, 127047, 491604, 799448, 464582,  148353,  208504, 700100, 968075,
    134107, 197284, 533990, 61835,  579261, 967278,  426528,  878251, 685287, 544269,
    588151, 692602, 62817,  39802,  90436,  220794,  470192,  585472, 695074, 765829,
    367285, 773998, 282654, 837142, 592651, 825299,  69507,   684433, 674883, 486001,
    785345, 629471, 476433, 842903, 752436, 47451,   574631,  328430, 190103, 227386};

/// @param items    : the maximum possible value to generate
/// @param theta    : how skewed the distribution is
/// @param seed     : the random seed
/// @param trace_length : the number of random numbers to generate
bool
test_zipfian_for_seed(const uint64_t items,
                      const double theta,
                      const uint64_t seed,
                      const uint64_t trace_length)
{
    foedus::assorted::ZipfianRandom zrng_oracle(items, theta, seed);
    ZipfianRandom zrng;
    zipfian_random_init(&zrng, items, theta, seed);

    for (uint64_t i = 0; i < trace_length; ++i) {
        uint64_t zipf_output = zipfian_random_next(&zrng);
        uint64_t zipf_oracle = zrng_oracle.next();
        if (zipf_output != zipf_oracle) {
            printf("[ERROR] %s:%d - on iteration %" PRIu64 "got %" PRIu64 ", expected %" PRIu64
                   "\n",
                   __FILE__,
                   __LINE__,
                   i,
                   zipf_output,
                   zipf_oracle);
            assert(0 && "zipfian output should match oracle!");
            return false;
        }
    }

    return true;
}

bool
test_zipfian()
{
    for (uint64_t i = 0; i < 10; ++i) {
        // NOTE The maximum possible number of items means that we get the
        //      maximum amount of information from the random-number generator.
        //      However, it also leads to impossibly long run-times. For this
        //      reason, we use a suitably (but not outrageously) large number.
        //      We should vary the theta; trace_length should be as long as
        //      reasonable.
        ASSERT_TRUE_OR_CLEANUP(
            test_zipfian_for_seed(1 << 20, 0.5, randomly_generated_seeds[i], 1000),
            printf("No cleanup required here!\n"));
    }
    return true;
}

int
main()
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_zipfian());
    return 0;
}
