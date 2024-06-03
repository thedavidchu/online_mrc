#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "hash/types.h"
#include "types/entry_type.h"

#include "priority_queue/splay_priority_queue.h"

struct SubtreeMultimap {
    Hash64BitType key;
    EntryType value;
    uint64_t cardinality;
    struct SubtreeMultimap *left_subtree;
    struct SubtreeMultimap *right_subtree;
};

#define compare(i, j) ((int64_t)(i) - (int64_t)(j))
/* This is the comparison.                                       */
/* Returns <0 if i<j, =0 if i=j, and >0 if i>j                   */

#define node_size(x) (((x) == NULL) ? 0 : ((x)->cardinality))
/* This macro returns the size of a node.  Unlike "x->cardinality",     */
/* it works even if x=NULL.  The test could be avoided by using  */
/* a special version of NULL which was a real node with cardinality 0.  */

static struct SubtreeMultimap *
sleator_splay(struct SubtreeMultimap *t, Hash64BitType key);

static struct SubtreeMultimap *
sleator_splay(struct SubtreeMultimap *t, Hash64BitType i)
/* Splay using the key i (which may or may not be in the tree.) */
/* The starting root is t, and the tree used is defined by rat  */
/* size fields are maintained */
{
    struct SubtreeMultimap N, *l, *r, *y;
    int64_t comp;
    uint64_t l_size, r_size;
    if (t == NULL)
        return t;
    N.left_subtree = N.right_subtree = NULL;
    l = r = &N;
    l_size = r_size = 0;

    for (;;) {
        comp = compare(i, t->key);
        if (comp < 0) {
            if (t->left_subtree == NULL)
                break;
            if (compare(i, t->left_subtree->key) < 0) {
                y = t->left_subtree; /* rotate right */
                t->left_subtree = y->right_subtree;
                y->right_subtree = t;
                t->cardinality = node_size(t->left_subtree) +
                                 node_size(t->right_subtree) + 1;
                t = y;
                if (t->left_subtree == NULL)
                    break;
            }
            r->left_subtree = t; /* link right */
            r = t;
            t = t->left_subtree;
            r_size += 1 + node_size(r->right_subtree);
        } else if (comp > 0) {
            if (t->right_subtree == NULL)
                break;
            if (compare(i, t->right_subtree->key) > 0) {
                y = t->right_subtree; /* rotate left */
                t->right_subtree = y->left_subtree;
                y->left_subtree = t;
                t->cardinality = node_size(t->left_subtree) +
                                 node_size(t->right_subtree) + 1;
                t = y;
                if (t->right_subtree == NULL)
                    break;
            }
            l->right_subtree = t; /* link left */
            l = t;
            t = t->right_subtree;
            l_size += 1 + node_size(l->left_subtree);
        } else {
            break;
        }
    }
    l_size +=
        node_size(t->left_subtree); /* Now l_size and r_size are the sizes of */
    r_size += node_size(
        t->right_subtree); /* the left and right trees we just built.*/
    t->cardinality = l_size + r_size + 1;

    l->right_subtree = r->left_subtree = NULL;

    /* The following two loops correct the size fields of the right path  */
    /* from the left child of the root and the right path from the left   */
    /* child of the root.                                                 */
    for (y = N.right_subtree; y != NULL; y = y->right_subtree) {
        y->cardinality = l_size;
        l_size -= 1 + node_size(y->left_subtree);
    }
    for (y = N.left_subtree; y != NULL; y = y->left_subtree) {
        y->cardinality = r_size;
        r_size -= 1 + node_size(y->right_subtree);
    }

    l->right_subtree = t->left_subtree; /* assemble */
    r->left_subtree = t->right_subtree;
    t->left_subtree = N.right_subtree;
    t->right_subtree = N.left_subtree;

    return t;
}

static bool
push_subtree(struct SplayPriorityQueue *me, struct SubtreeMultimap *subtree)
{
    if (me == NULL || me->num_free_subtrees == me->max_cardinality) {
        return false;
    }
    const uint64_t index = me->num_free_subtrees;
    ++me->num_free_subtrees;
    me->free_subtrees[index] = subtree;
    return true;
}

static struct SubtreeMultimap *
pop_subtree(struct SplayPriorityQueue *me)
{
    if (me == NULL || me->num_free_subtrees == 0) {
        // NOTE The free_subtrees pointer should still be non-null because it is
        //      just a buffer.
        return NULL;
    }
    assert(me->free_subtrees != NULL && "free subtrees should not be zero");
    const uint64_t index = me->num_free_subtrees - 1;
    --me->num_free_subtrees;
    return me->free_subtrees[index];
}

bool
SplayPriorityQueue__init(struct SplayPriorityQueue *me,
                         const uint64_t max_cardinality)
{
    if (me == NULL || max_cardinality == 0) {
        return false;
    }
    me->root = NULL;
    me->cardinality = 0;

    me->all_subtrees =
        (struct SubtreeMultimap *)calloc(max_cardinality,
                                         sizeof(struct SubtreeMultimap));
    if (me->all_subtrees == NULL) {
        return false;
    }
    me->free_subtrees = (struct SubtreeMultimap **)malloc(
        max_cardinality * sizeof(struct SubtreeMultimap *));
    if (me->free_subtrees == NULL) {
        free(me->all_subtrees);
        return false;
    }
    for (uint64_t i = 0; i < max_cardinality; ++i) {
        me->free_subtrees[i] = &me->all_subtrees[i];
    }
    me->num_free_subtrees = max_cardinality;
    me->max_cardinality = max_cardinality;
    return true;
}

bool
SplayPriorityQueue__is_full(struct SplayPriorityQueue *me)
{
    if (me == NULL) {
        return true;
    }
    return me->cardinality == me->max_cardinality;
}

bool
SplayPriorityQueue__insert_if_room(struct SplayPriorityQueue *me,
                                   const Hash64BitType hash,
                                   const EntryType entry)
{
    struct SubtreeMultimap *new_subtree;
    if (me == NULL || me->num_free_subtrees == 0) {
        return false;
    }
    if (me->root != NULL) {
        me->root = sleator_splay(me->root, hash);
        if (me->root->key == hash && me->root->value == entry) {
            // Key-value pair is already present!
            return false;
        }
    }
    new_subtree = pop_subtree(me);
    if (new_subtree == NULL) {
        return false;
    }
    if (me->root == NULL) {
        new_subtree->left_subtree = NULL;
        new_subtree->right_subtree = NULL;
    } else if (hash < me->root->key) {
        new_subtree->left_subtree = me->root->left_subtree;
        new_subtree->right_subtree = me->root;
        me->root->left_subtree = NULL;
        me->root->cardinality = 1 + node_size(me->root->right_subtree);
    } else {
        new_subtree->right_subtree = me->root->right_subtree;
        new_subtree->left_subtree = me->root;
        me->root->right_subtree = NULL;
        me->root->cardinality = 1 + node_size(me->root->left_subtree);
    }
    new_subtree->key = hash;
    new_subtree->value = entry;
    new_subtree->cardinality = 1 + node_size(new_subtree->left_subtree) +
                               node_size(new_subtree->right_subtree);
    // Update tree data structure
    me->root = new_subtree;
    me->cardinality = new_subtree->cardinality;
    return true;
}

Hash64BitType
SplayPriorityQueue__get_max_hash(struct SplayPriorityQueue *me)
{
    if (me == NULL || me->root == NULL) {
        assert(me->cardinality == 0 && "cardinality should be zero!");
        return 0;
    }
    assert(me->cardinality != 0 && "cardinality should be non-zero!");
    me->root = sleator_splay(me->root, SIZE_MAX);

    struct SubtreeMultimap *max_hash_node = me->root;
    while (true) {
        if (max_hash_node->right_subtree == NULL) {
            return max_hash_node->key;
        }
        max_hash_node = max_hash_node->right_subtree;
    }
}

bool
SplayPriorityQueue__remove(struct SplayPriorityQueue *me,
                           Hash64BitType largest_hash,
                           EntryType *entry)
{
    struct SubtreeMultimap *t;
    struct SubtreeMultimap *x;

    if (me == NULL || me->root == NULL) {
        return false;
    }
    me->root = sleator_splay(me->root, largest_hash);
    t = me->root;
    if (compare(largest_hash, t->key) != 0) {
        return false; /* It wasn't there */
    }
    /* found it */
    if (t->left_subtree == NULL) {
        x = t->right_subtree;
    } else {
        x = sleator_splay(t->left_subtree, largest_hash);
        x->right_subtree = t->right_subtree;
    }
    *entry = t->value;
    push_subtree(me, t);
    if (x == NULL) {
        me->cardinality = 0;
    } else {
        x->cardinality = me->cardinality - 1;
        me->cardinality = x->cardinality;
    }
    me->root = x;
    return true;
}

void
SplayPriorityQueue__destroy(struct SplayPriorityQueue *me)
{
    if (me == NULL) {
        return;
    }
    free(me->free_subtrees);
    free(me->all_subtrees);

    /* TODO free me->root */

    *me = (struct SplayPriorityQueue){0};
    return;
}
