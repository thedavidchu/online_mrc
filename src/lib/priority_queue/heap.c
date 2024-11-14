#include <assert.h>
#include <inttypes.h>
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
// NOTE I realized that I can actually sort-of mimic generics by using a
//      generic 'KeyType' and use the build system to connect me to
//      different 'KeyTypes'. This sounds like a convoluted mess that
//      will bite me in the future, but until then, YAY!
#include "types/key_type.h"

/// @brief  Returns if lhs is greater than rhs.
static inline bool
gt(KeyType const lhs, KeyType const rhs)
{
    return lhs > rhs;
}

/// @brief  Returns if lhs is less than rhs.
static inline bool
lt(KeyType const lhs, KeyType const rhs)
{
    return lhs < rhs;
}

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
static inline KeyType
get_priority_or(struct Heap const *const me,
                size_t const idx,
                KeyType const otherwise)
{
    assert(me && me->data);
    return idx < me->length ? me->data[idx].key : otherwise;
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
    if (!me->cmp(me->data[idx].key, me->data[get_parent(idx)].key))
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
    KeyType my_priority = get_priority_or(me, idx, me->bottom_key);
    // We may either have: 2 children, a left child, or no children.
    size_t const left_child = get_left_child(idx);
    size_t const right_child = get_right_child(idx);
    KeyType left_child_priority =
        get_priority_or(me, left_child, me->bottom_key);
    KeyType right_child_priority =
        get_priority_or(me, right_child, me->bottom_key);

    // NOTE I bias toward searching the right size (if they are equal)
    //      simply because I want to use the 'cmp()' function in the
    //      simplist way possible.
    if (me->cmp(left_child_priority, right_child_priority)) {
        if (me->cmp(left_child_priority, my_priority)) {
            swap(me, idx, left_child);
            sift_down(me, left_child);
        }
    } else {
        if (me->cmp(right_child_priority, my_priority)) {
            swap(me, idx, right_child);
            sift_down(me, right_child);
        }
    }
}

/// @note   This is expensive, since we are validating the entire heap.
static bool
validate_heap_property(struct Heap const *const me)
{
    assert(me != NULL && me->cmp != NULL);
    assert(implies(me->length != 0, me->data != NULL));
    assert(implies(me->cmp == gt, me->bottom_key == 0));
    assert(implies(me->cmp == lt, me->bottom_key == SIZE_MAX));
    assert(me->length <= me->capacity);

    bool ok = true;
    for (size_t i = 0; i < me->length; ++i) {
        KeyType my_priority = get_priority_or(me, i, me->bottom_key);
        KeyType left_priority =
            get_priority_or(me, get_left_child(i), me->bottom_key);
        KeyType right_priority =
            get_priority_or(me, get_right_child(i), me->bottom_key);

        if (!me->cmp(my_priority, left_priority)) {
            LOGGER_ERROR(
                "at position %zu, my priority (%" PRIu64
                ") must be greater than left child's priority (%" PRIu64 ")",
                i,
                my_priority,
                left_priority);
            ok = false;
        }
        if (!me->cmp(my_priority, right_priority)) {
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
    if (!validate_heap_property(me)) {
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
            "{\".key\": %" PRIu64 ", \".value\": %" PRIu64 "}",
            item.key,
            item.value);
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
Heap__init_max_heap(struct Heap *me, size_t const max_size)
{
    if (me == NULL) {
        return false;
    }

    *me = (struct Heap){.data = calloc(max_size, sizeof(struct HeapItem)),
                        .length = 0,
                        .capacity = max_size,
                        .cmp = gt,
                        .bottom_key = 0};

    if (!validate_metadata(me)) {
        LOGGER_ERROR("init failed");
        return false;
    }
    return true;
}

bool
Heap__init_min_heap(struct Heap *me, size_t const max_size)
{
    if (me == NULL) {
        return false;
    }

    *me = (struct Heap){.data = calloc(max_size, sizeof(struct HeapItem)),
                        .length = 0,
                        .capacity = max_size,
                        .cmp = lt,
                        .bottom_key = SIZE_MAX};

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
        // My reasoning is that NULL is implicitly full... I'll admit
        // this is confusing semantics.
        return true;
    }
    assert(me->length <= me->capacity);
    return me->length == me->capacity;
}

bool
Heap__insert_if_room(struct Heap *const me,
                     const KeyType key,
                     const ValueType value)
{
    if (me == NULL || Heap__is_full(me)) {
        return false;
    }

    size_t target = me->length;
    ++(me->length);

    me->data[target] = (struct HeapItem){.key = key, .value = value};
    sift_up(me, target);
    return true;
}

bool
Heap__insert(struct Heap *const me, const KeyType key, const ValueType value)
{
    if (me == NULL) {
        return false;
    }
    if (Heap__is_full(me)) {
        struct HeapItem *const tmp =
            realloc(me->data, 2 * me->capacity * sizeof(*tmp));
        if (tmp == NULL) {
            LOGGER_ERROR("failed to reallocate");
            return false;
        }
        me->data = tmp;
        me->capacity = 2 * me->capacity;
    }

    size_t target = me->length;
    ++(me->length);

    me->data[target] = (struct HeapItem){.key = key, .value = value};
    sift_up(me, target);
    return true;
}

KeyType
Heap__get_top_key(struct Heap *me)
{
    if (me == NULL || me->data == NULL || me->length == 0) {
        return 0;
    }

    return me->data[0].key;
}

bool
Heap__remove(struct Heap *me, KeyType rm_key, ValueType *value_return)
{
    if (me == NULL || me->length == 0) {
        return false;
    }

    if (me->data[0].key != rm_key)
        return false;
    *value_return = me->data[0].value;
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
