#include <assert.h>
#include <stdbool.h>

#include <glib.h>

#include "arrays/reverse_index.h"
#include "hash/types.h"
#include "types/entry_type.h"

#include "priority_queue/heap.h"

// NOTE Due to my sheer laziness, this function leaks memory upon failure!
//      I think my other testing functions likely leak memory upon failure too;
//      however in the others, I at least try to clean up memory upon failure.
// TODO Call this function with g_test_add(...) which includes facilities
//      for cleanup.
static void
test_static_max_heap(void)
{
    struct Heap pq;

    g_assert_true(Heap__init_max_heap(&pq, 10));

    for (Hash64BitType i = 0; i < 10; ++i) {
        // The pq should not be full until we reach the end of this loop!
        g_assert_false(Heap__is_full(&pq));
        g_assert_true(
            Heap__insert_if_room(&pq, (Hash64BitType)i, (EntryType)i));
    }
    // The pq should now be full!
    g_assert_true(Heap__is_full(&pq));
    // This is kind of a hack but it's because I am making my own macro library
    // and don't want to have to define a TRUE and FALSE version.
    g_assert_false(Heap__insert_if_room(&pq, (Hash64BitType)10, (EntryType)10));

    // Get maximum
    Hash64BitType hash = Heap__get_top_key(&pq);
    g_assert_cmpuint(hash, ==, 9);

    // Remove maximum
    EntryType entry = 0;
    g_assert_true(Heap__remove(&pq, hash, &entry));
    g_assert_cmpuint(entry, ==, 9);
    g_assert_false(Heap__remove(&pq, hash, &entry));

    // Insert and remove duplicate
    hash = 8;
    entry = 9;
    g_assert_false(Heap__is_full(&pq));
    g_assert_true(Heap__insert_if_room(&pq, hash, entry));
    EntryType entry_0, entry_1;
    hash = Heap__get_top_key(&pq);
    g_assert_cmpuint(hash, ==, 8);
    g_assert_true(Heap__remove(&pq, hash, &entry_0));
    hash = Heap__get_top_key(&pq);
    g_assert_cmpuint(hash, ==, 8);
    g_assert_true(Heap__remove(&pq, hash, &entry_1));
    hash = Heap__get_top_key(&pq);
    g_assert_cmpuint(hash, ==, 7);
    g_assert_true((entry_0 == 8 && entry_1 == 9) ||
                  (entry_0 == 9 && entry_1 == 8));

    for (uint64_t i = 0; i < 7; ++i) {
        Hash64BitType expected_max_hash = 7 - i;
        g_assert_false(Heap__is_full(&pq));
        hash = Heap__get_top_key(&pq);
        g_assert_cmpuint(hash, ==, expected_max_hash);
        // Remove
        g_assert_true(Heap__remove(&pq, expected_max_hash, &entry));
        g_assert_cmpuint(entry, ==, (EntryType)expected_max_hash);
        // Try another removal that should fail
        hash = Heap__get_top_key(&pq);
        g_assert_cmpuint(hash, ==, expected_max_hash - 1);
        g_assert_false(Heap__remove(&pq, expected_max_hash, &entry));
    }
    Heap__destroy(&pq);
}

static void
test_big_static_max_heap(void)
{
    size_t const heap_size = 1 << 12;
    struct Heap me = {0};
    g_assert_true(Heap__init_max_heap(&me, heap_size));

    for (size_t i = 0; i < 1 << 12; ++i) {
        g_assert_true(Heap__insert_if_room(&me, i, i));
    }

    g_assert_true(Heap__is_full(&me));
    g_assert_cmpuint(Heap__get_top_key(&me), ==, heap_size - 1);
    for (size_t i = 0; i < heap_size; ++i) {
        size_t max_key = REVERSE_INDEX(i, heap_size);
        size_t max_val = 0;
        g_assert_cmpuint(Heap__get_top_key(&me), ==, max_key);
        g_assert_true(Heap__remove(&me, max_key, &max_val));
        g_assert_cmpuint(max_val, ==, max_key);
        g_assert_false(Heap__is_full(&me));
    }

    g_assert_cmpuint(me.length, ==, 0);
}

static void
test_big_dynamic_min_heap(void)
{
    size_t const original_heap_size = 1 << 4;
    size_t const full_heap_size = 1 << 12;
    struct Heap me = {0};
    g_assert_true(Heap__init_min_heap(&me, original_heap_size));

    for (size_t i = 0; i < full_heap_size; ++i) {
        g_assert_true(Heap__insert(&me, i, i));
    }

    // NOTE This is true because we resize by factors of two. This is
    //      not otherwise necessary to maintain the heap property.
    g_assert_true(Heap__is_full(&me));
    g_assert_cmpuint(Heap__get_top_key(&me), ==, 0);
    for (size_t i = 0; i < full_heap_size; ++i) {
        size_t max_key = i;
        size_t max_val = 0;
        g_assert_cmpuint(Heap__get_top_key(&me), ==, max_key);
        g_assert_true(Heap__remove(&me, max_key, &max_val));
        g_assert_cmpuint(max_val, ==, max_key);
        g_assert_false(Heap__is_full(&me));
    }

    g_assert_cmpuint(me.length, ==, 0);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/priority_queue_test/heap__test", test_static_max_heap);
    g_test_add_func("/priority_queue_test/heap__big_test",
                    test_big_static_max_heap);
    g_test_add_func("/priority_queue_test/heap__big_dynamic_min_test",
                    test_big_dynamic_min_heap);
    return g_test_run();
}
