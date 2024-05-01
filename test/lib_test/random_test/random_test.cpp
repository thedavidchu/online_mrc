#include "unused/mark_unused.h"
#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <cstdio>

// HACK This is a complete hack just to get the C++ linker to work. AHHH!
extern "C" {
// TODO(dchu)   Test the Uniform Random function directly
// I don't test the uniform random function directly... yet.
#include "random/uniform_random.h"
#include "random/zipfian_random.h"
}

#include "arrays/array_size.h"
#include "test/mytester.h"

// TODO(dchu)   Test the Uniform Random function directly
// I don't test the uniform random function directly... yet.
#include "random_test/uniform_random.hpp"
#include "random_test/zipfian_random.hpp"

// NOTE Generated with the following Python script:
//      import random; [random.random() for _ in range(100)]
double randomly_generated_thetas[100] = {
    0.6499073375808043,  0.6827406196202658,   0.7560234464648662,
    0.21761327474897285, 0.35953878075597634,  0.5981401377391279,
    0.30051849894559624, 0.15198137351561491,  0.9433482319297957,
    0.5175761850998114,  0.7919630001566174,   0.5390868499465348,
    0.784233908326908,   0.6614215519777658,   0.6469181187109458,
    0.4497433238648352,  0.22774657077536076,  0.7200512075923744,
    0.8676575020821076,  0.41670745734286596,  0.9828727829664993,
    0.8490030758276803,  0.7810605952349712,   0.1669021059176241,
    0.6722523637195411,  0.053511203792582895, 0.16753622724528938,
    0.7337020430417065,  0.2898306369975815,   0.5649403524328462,
    0.6293248538499115,  0.24890710144673023,  0.21835125309582726,
    0.2894381025207047,  0.8011068983154949,   0.11285465040687837,
    0.7181120580035528,  0.523418245117352,    0.8835524999163235,
    0.10684718726639975, 0.6486655984091683,   0.8098809405741204,
    0.3525955664257755,  0.7145596021170604,   0.8313407107244092,
    0.8073620821803092,  0.027724620419275703, 0.11526393649493649,
    0.7458021098974952,  0.3528827440076491,   0.2750089756556836,
    0.2840251664244039,  0.8164867343286026,   0.761524118691336,
    0.4413694656445849,  0.19562200947252217,  0.7501262895672682,
    0.6602948761472196,  0.5143018747729589,   0.30761974095291744,
    0.41650457496231297, 0.2963899103109142,   0.5280464448618983,
    0.7498449734745987,  0.7433326156491723,   0.982379184755697,
    0.8768043493085406,  0.31011471920803935,  0.36375710848674403,
    0.0810335380918622,  0.40195408879671,     0.3169288676874231,
    0.26386742826974763, 0.5921881847561036,   0.48485454447912135,
    0.5006169714652716,  0.432852488399324,    0.1379918765663546,
    0.5732230997048371,  0.000739781496123415, 0.5969451762653092,
    0.8832109060398151,  0.12237421767048273,  0.2719035843552128,
    0.7463672578821582,  0.6175433017781593,   0.7860204680227317,
    0.08239588474686221, 0.07785770960177774,  0.31293756967082786,
    0.8634406909765703,  0.1747738864720474,   0.44602184683936097,
    0.8546837490065481,  0.4218506719761447,   0.431401666397358,
    0.7678094679197528,  0.8559958945760421,   0.3084612187003727,
    0.5474990685054882};
// NOTE Generated with the following Python script:
//      import random; [random.randint(0, 1 << 20) for _ in range(100)]
uint64_t randomly_generated_bounds[100] = {
    300805,  257636, 40593,  319279, 953626,  175607, 683881,  360451, 868887,
    158086,  772344, 534372, 121940, 280388,  826475, 241367,  838862, 183864,
    781258,  739880, 514810, 274602, 894301,  349339, 1046646, 888555, 570321,
    556944,  16663,  433136, 588088, 218292,  15371,  835823,  347819, 585130,
    877326,  101437, 674047, 597305, 750974,  104496, 635662,  648640, 108489,
    998342,  575051, 374714, 583626, 570329,  788624, 102972,  429379, 82660,
    518794,  348510, 835384, 879664, 526365,  102863, 453699,  157829, 774708,
    1033496, 753518, 197524, 35710,  267832,  255584, 613672,  718828, 727662,
    339374,  7753,   611772, 435824, 574821,  805482, 667514,  415253, 480407,
    533931,  916986, 757577, 793043, 1034636, 36631,  472806,  227444, 962470,
    724589,  882023, 981607, 664037, 244284,  339029, 274794,  134756, 54066,
    371188};
uint64_t randomly_generated_seeds[100] = {
    141660,  480415,  620145, 329492, 645058, 688290, 142414,  565032, 269066,
    542702,  935407,  940243, 741758, 7487,   929452, 1024453, 649212, 596986,
    488264,  341134,  325642, 780511, 619697, 60228,  252594,  667931, 506263,
    179048,  439014,  38977,  407685, 734321, 961367, 842433,  875855, 61421,
    1016821, 469277,  102209, 161102, 97616,  425410, 174331,  252233, 25582,
    575849,  1030875, 523705, 874388, 288983, 932377, 214718,  127047, 491604,
    799448,  464582,  148353, 208504, 700100, 968075, 134107,  197284, 533990,
    61835,   579261,  967278, 426528, 878251, 685287, 544269,  588151, 692602,
    62817,   39802,   90436,  220794, 470192, 585472, 695074,  765829, 367285,
    773998,  282654,  837142, 592651, 825299, 69507,  684433,  674883, 486001,
    785345,  629471,  476433, 842903, 752436, 47451,  574631,  328430, 190103,
    227386};

/// @param items    : the maximum possible value to generate
/// @param theta    : how skewed the distribution is
/// @param seed     : the random seed
/// @param trace_length : the number of random numbers to generate
static bool
test_zipfian_for_seed(const uint64_t items,
                      const double theta,
                      const uint64_t seed,
                      const uint64_t trace_length)
{
    foedus::assorted::ZipfianRandom zrng_oracle(items, theta, seed);
    ZipfianRandom zrng;
    ZipfianRandom__init(&zrng, items, theta, seed);

    for (uint64_t i = 0; i < trace_length; ++i) {
        uint64_t zipf_output = ZipfianRandom__next(&zrng);
        uint64_t zipf_oracle = zrng_oracle.next();
        if (zipf_output != zipf_oracle) {
            printf("[ERROR] %s:%d - on iteration %" PRIu64 "got %" PRIu64
                   ", expected %" PRIu64 "\n",
                   __FILE__,
                   __LINE__,
                   i,
                   zipf_output,
                   zipf_oracle);
            assert(0 && "zipfian output should match oracle!");
            return false;
        }
    }
    ZipfianRandom__destroy(&zrng);

    return true;
}

static bool
test_zipfian(void)
{
    for (uint64_t i = 0; i < ARRAY_SIZE(randomly_generated_bounds); ++i) {
        // NOTE The maximum possible number of items means that we get the
        //      maximum amount of information from the random-number generator.
        //      However, it also leads to impossibly long run-times. For this
        //      reason, we use a suitably (but not outrageously) large number.
        //      We should vary the theta; trace_length should be as long as
        //      reasonable.
        assert(test_zipfian_for_seed(randomly_generated_bounds[i],
                                     randomly_generated_thetas[i],
                                     randomly_generated_seeds[i],
                                     1000));
    }
    return true;
}

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    ASSERT_FUNCTION_RETURNS_TRUE(test_zipfian());
    return 0;
}
