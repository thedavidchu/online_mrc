#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "arrays/is_last.h"
#include "hash/types.h"
#include "invariants/implies.h"
#include "logger/logger.h"
#include "priority_queue/heap.h"

static inline size_t
get_right_child(size_t const i)
{
    return 2 * i + 2;
}

static inline size_t
get_left_child(size_t const i)
{
    return 2 * i + 1;
}

static inline size_t
get_parent(size_t const i)
{
    return (i - 1) / 2;
}

/// @brief  Get the priority of an index or return the default value.
static inline Hash64BitType
get_priority_or(struct Heap const *const me,
                size_t const idx,
                Hash64BitType const otherwise)
{
    assert(me && me->data);
    return idx < me->length ? me->data[idx].priority : otherwise;
}

static inline void
swap(struct Heap *const me, size_t const i, size_t const j)
{
    assert(me && me->data && i < me->length && j < me->length);
    struct HeapItem tmp = me->data[i];
    me->data[i] = me->data[j];
    me->data[j] = tmp;
}

static inline void
sift_up(struct Heap *const me, size_t const idx)
{
    assert(me);
    if (idx == 0)
        return;
    if (me->data[idx].priority <= me->data[get_parent(idx)].priority)
        return;
    swap(me, idx, get_parent(idx));
    sift_up(me, get_parent(idx));
}

/// @brief  Sift a small node down the heap.
/// @details    This assumes the max root has been evicted and a relatively
///             small priority value has replaced it.
static inline void
sift_down(struct Heap *const me, size_t const idx)
{
    assert(me);
    if (idx >= me->length - 1)
        return;
    Hash64BitType my_priority = get_priority_or(me, idx, 0);
    // We may either have: 2 children, a left child, or no children.
    size_t const left_child = get_left_child(idx);
    size_t const right_child = get_right_child(idx);
    Hash64BitType left_child_priority = get_priority_or(me, left_child, 0);
    Hash64BitType right_child_priority = get_priority_or(me, right_child, 0);

    if (left_child_priority >= right_child_priority) {
        if (left_child_priority > my_priority) {
            swap(me, idx, left_child);
            sift_down(me, left_child);
        }
    } else {
        if (right_child_priority > my_priority) {
            swap(me, idx, right_child);
            sift_down(me, right_child);
        }
    }
}

/// @note   This is expensive, since we are validating the entire heap.
static bool
validate_max_heap_property(struct Heap const *const me)
{
    assert(implies(me->length != 0, me->data != NULL));
    assert(me->length <= me->capacity);

    bool ok = true;
    for (size_t i = 0; i < me->length; ++i) {
        Hash64BitType my_priority = get_priority_or(me, i, 0);
        Hash64BitType left_priority = get_priority_or(me, get_left_child(i), 0);
        Hash64BitType right_priority =
            get_priority_or(me, get_right_child(i), 0);

        if (my_priority < left_priority) {
            LOGGER_ERROR(
                "at position %zu, my priority (%" PRIu64
                ") must be greater than left child's priority (%" PRIu64 ")",
                i,
                my_priority,
                left_priority);
            ok = false;
        }
        if (my_priority < right_priority) {
            LOGGER_ERROR(
                "at position %zu, my priority (%" PRIu64
                ") must be greater than right child's priority (%" PRIu64 ")",
                i,
                my_priority,
                right_priority);
            ok = false;
        }
    }
    return ok;
}

static bool
validate_metadata(struct Heap const *const me)
{
    // Being NULL is consistent even if I can't do anything with it.
    if (me == NULL) {
        LOGGER_WARN("got NULL in consistency validation");
        return true;
    }
    if (!implies(me->capacity != 0, me->data != NULL)) {
        LOGGER_ERROR("calloc failed");
        return false;
    }
    if (me->length > me->capacity) {
        LOGGER_ERROR("length (%zu) must be less than capacity (%zu)",
                     me->length,
                     me->capacity);
        return false;
    }
    return true;
}

bool
Heap__validate(struct Heap const *const me)
{
    if (!validate_metadata(me)) {
        LOGGER_ERROR("invalid heap metadata");
        return false;
    }
    if (!validate_max_heap_property(me)) {
        LOGGER_ERROR("invalid max heap");
        return false;
    }
    return true;
}

static inline void
write_heap_item(FILE *stream, struct HeapItem item)
{
    assert(stream != NULL);
    fprintf(stream,
            "{\".priority\": %" PRIu64 ", \".entry\": %" PRIu64 "}",
            item.priority,
            item.entry);
}

bool
Heap__write_as_json(FILE *stream, struct Heap const *const me)
{
    if (stream == NULL) {
        LOGGER_ERROR("invalid stream");
        return false;
    }
    if (me == NULL) {
        fprintf(stream, "{\"type\": null}\n");
        return false;
    }
    if (me->data == NULL) {
        fprintf(stream, "{\"type\": \"Heap\", \".data\": null}\n");
        return false;
    }
    fprintf(stream,
            "{\"type\": \"Heap\", \".length\": %zu, \".capacity\": %zu, "
            "\".data\": [",
            me->length,
            me->capacity);
    for (size_t i = 0; i < me->length; ++i) {
        write_heap_item(stream, me->data[i]);
        if (!is_last(i, me->length)) {
            fprintf(stream, ", ");
        }
    }
    fprintf(stream, "]}\n");
    return true;
}

bool
Heap__init(struct Heap *me, size_t const max_size)
{
    if (me == NULL) {
        return false;
    }

    *me = (struct Heap){.data = calloc(max_size, sizeof(struct HeapItem)),
                        .length = 0,
                        .capacity = max_size};

    if (!validate_metadata(me)) {
        LOGGER_ERROR("init failed");
        return false;
    }
    return true;
}

bool
Heap__is_full(struct Heap const *const me)
{
    if (me == NULL) {
        assert(validate_metadata(me));
        // My reasoning is that NULL is implicitly full... I'll admit
        // this is confusing semantics.
        return true;
    }
    assert(me->length <= me->capacity);
    return me->length == me->capacity;
}

bool
Heap__insert_if_room(struct Heap *const me,
                     const Hash64BitType hash,
                     const EntryType entry)
{
    if (me == NULL || Heap__is_full(me)) {
        return false;
    }

    size_t target = me->length;
    ++(me->length);

    me->data[target] = (struct HeapItem){.priority = hash, .entry = entry};
    sift_up(me, target);
    return true;
}

Hash64BitType
Heap__get_max_hash(struct Heap *me)
{
    if (me == NULL || me->data == NULL || me->length == 0) {
        assert(validate_metadata(me));
        return 0;
    }

    return me->data[0].priority;
}

bool
Heap__remove(struct Heap *me, Hash64BitType largest_hash, EntryType *entry)
{
    if (me == NULL || me->length == 0) {
        return false;
    }

    if (me->data[0].priority != largest_hash)
        return false;
    *entry = me->data[0].entry;
    size_t const last = me->length - 1;
    me->data[0] = me->data[last];
    --(me->length);
    sift_down(me, 0);
    return true;
}

void
Heap__destroy(struct Heap *me)
{
    if (me == NULL)
        return;
    free(me->data);
    *me = (struct Heap){0};
}
