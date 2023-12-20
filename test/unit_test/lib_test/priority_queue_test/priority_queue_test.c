#include <assert.h>
#include <stdbool.h>

#include <glib.h>

#include "hash/types.h"
#include "types/entry_type.h"

#include "priority_queue/splay_priority_queue.h"

// NOTE Due to my sheer laziness, this function leaks memory upon failure!
//      I think my other testing functions likely leak memory upon failure too;
//      however in the others, I at least try to clean up memory upon failure.
// TODO Call this function with g_test_add(...) which includes facilities
//      for cleanup.
void
test_splay_priority_queue(void)
{
    struct SplayPriorityQueue pq;

    g_assert_true(splay_priority_queue__init(&pq, 10));

    for (Hash64BitType i = 0; i < 10; ++i) {
        // The pq should not be full until we reach the end of this loop!
        g_assert_false(splay_priority_queue__is_full(&pq));
        g_assert_true(splay_priority_queue__insert_if_room(&pq,
                                                           (Hash64BitType)i,
                                                           (EntryType)i));
    }
    // The pq should now be full!
    g_assert_true(splay_priority_queue__is_full(&pq));
    // This is kind of a hack but it's because I am making my own macro library
    // and don't want to have to define a TRUE and FALSE version.
    g_assert_false(splay_priority_queue__insert_if_room(&pq,
                                                        (Hash64BitType)10,
                                                        (EntryType)10));

    // Get maximum
    Hash64BitType hash = splay_priority_queue__get_max_hash(&pq);
    g_assert_cmpuint(hash, ==, 9);

    // Remove maximum
    EntryType entry = 0;
    g_assert_true(splay_priority_queue__remove(&pq, hash, &entry));
    g_assert_cmpuint(entry, ==, 9);
    g_assert_false(splay_priority_queue__remove(&pq, hash, &entry));

    // Insert and remove duplicate
    hash = 8;
    entry = 9;
    g_assert_false(splay_priority_queue__is_full(&pq));
    g_assert_true(splay_priority_queue__insert_if_room(&pq, hash, entry));
    EntryType entry_0, entry_1;
    hash = splay_priority_queue__get_max_hash(&pq);
    g_assert_cmpuint(hash, ==, 8);
    g_assert_true(splay_priority_queue__remove(&pq, hash, &entry_0));
    hash = splay_priority_queue__get_max_hash(&pq);
    g_assert_cmpuint(hash, ==, 8);
    g_assert_true(splay_priority_queue__remove(&pq, hash, &entry_1));
    hash = splay_priority_queue__get_max_hash(&pq);
    g_assert_cmpuint(hash, ==, 7);
    g_assert_true((entry_0 == 8 && entry_1 == 9) ||
                  (entry_0 == 9 && entry_1 == 8));

    for (uint64_t i = 0; i < 7; ++i) {
        Hash64BitType expected_max_hash = 7 - i;
        g_assert_false(splay_priority_queue__is_full(&pq));
        hash = splay_priority_queue__get_max_hash(&pq);
        g_assert_cmpuint(hash, ==, expected_max_hash);
        // Remove
        g_assert_true(
            splay_priority_queue__remove(&pq, expected_max_hash, &entry));
        g_assert_cmpuint(entry, ==, (EntryType)expected_max_hash);
        // Try another removal that should fail
        hash = splay_priority_queue__get_max_hash(&pq);
        g_assert_cmpuint(hash, ==, expected_max_hash - 1);
        g_assert_false(
            splay_priority_queue__remove(&pq, expected_max_hash, &entry));
    }
    splay_priority_queue__destroy(&pq);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/priority_queue_test/splay_priority_queue__test",
                    test_splay_priority_queue);
    return g_test_run();
}
