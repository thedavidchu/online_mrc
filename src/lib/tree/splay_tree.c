#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "tree/splay_tree.h"

struct NaiveSubtree {
    KeyType key;
    size_t cardinality;
    struct NaiveSubtree *left_subtree;
    struct NaiveSubtree *right_subtree;
};

struct Tree {
    struct NaiveSubtree *root;
    size_t cardinality;
};

struct RemoveStatus {
    struct NaiveSubtree *removed;
    struct NaiveSubtree *new_child;
};

static bool
subtree_splayed_remove(struct NaiveSubtree *me, KeyType key)
{
    struct NaiveSubtree **next_subtree;
    if (me == NULL) {
        // N.B. This should only be called if the first invocation passes a NULL
        return false;
    } else if (key < me->key) {
        next_subtree = &me->left_subtree;
    } else if (me->key < key) {
        next_subtree = &me->right_subtree;
    } else {
        return false;
    }

    if (*next_subtree == NULL) {
        *next_subtree = subtree_new(key);
        if (*next_subtree == NULL) {
            return false; // OOM error!
        }
        ++me->cardinality;
        return true;
    } else {
        bool r = subtree_splayed_remove(*next_subtree, key);
        if (r) {
            ++me->cardinality;
        }
        return r;
    }
}

bool
tree_naive_insert(struct Tree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    }
    if (me->root == NULL) {
        me->root = subtree_new(key);
        if (me->root == NULL) {
            return false;
        }
        ++me->cardinality;
        return true;
    }
    bool r = subtree_splayed_remove(me->root, key);
    if (r) {
        ++me->cardinality;
    }
    return r;
}

static struct RemoveStatus
subtree_pop_minimum(struct NaiveSubtree *me)
{
    if (me == NULL) {
        // NOTE This should only be called if this function is originally called
        //      with a NULL argument. No recursive call should pass NULL.
        return (struct RemoveStatus){.removed = NULL, .new_child = NULL};
    } else if (me->left_subtree == NULL) {
        return (struct RemoveStatus){.removed = me, .new_child = me->right_subtree};
    } else {
        struct RemoveStatus r = subtree_pop_minimum(me->left_subtree);
        assert(r.removed != NULL && "should have popped an item from the non-empty subtree");
        --me->cardinality;
        me->left_subtree = r.new_child;
        r.new_child = me;
        return r;
    }
}

static struct RemoveStatus
subtree_splayed_remove(struct NaiveSubtree *me, KeyType key)
{
    if (me == NULL) {
        return (struct RemoveStatus){.removed = false, .new_child = NULL};
    } else if (key < me->key) {
        struct RemoveStatus r = subtree_splayed_remove(me->left_subtree, key);
        if (r.removed) {
            --me->cardinality;
        }
        // NOTE Automatically overwriting the rather than checking may cause the
        //      cache line to be needlessly dirtied.
        me->left_subtree = r.new_child;
        r.new_child = me;
        return r;
    } else if (me->key < key) {
        struct RemoveStatus r = subtree_splayed_remove(me->right_subtree, key);
        if (r.removed) {
            --me->cardinality;
        }
        // NOTE Automatically overwriting the rather than checking may cause the
        //      cache line to be needlessly dirtied.
        me->right_subtree = r.new_child;
        r.new_child = me;
        return r;
    } else {
        if (me->left_subtree == NULL && me->right_subtree == NULL) {
            // Is a leaf
            return (struct RemoveStatus){.removed = me, .new_child = NULL};
        } else if (me->left_subtree == NULL) {
            // Single child
            struct NaiveSubtree *single_child = me->right_subtree;
            return (struct RemoveStatus){.removed = me, .new_child = single_child};
        } else if (me->right_subtree == NULL) {
            // Single child
            struct NaiveSubtree *single_child = me->left_subtree;
            return (struct RemoveStatus){.removed = me, .new_child = single_child};
        } else {
            // Double child => recursively replace with successor (arbitrarily
            // chose the successor rather than the predecessor).
            struct RemoveStatus r = subtree_pop_minimum(me->right_subtree);
            struct NaiveSubtree *replacement_node = r.removed;
            replacement_node->cardinality = me->cardinality - 1;
            replacement_node->left_subtree = me->left_subtree;
            replacement_node->right_subtree = r.new_child;
            return (struct RemoveStatus){.removed = me, .new_child = replacement_node};
        }
    }
}

bool
tree_naive_remove(struct Tree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    }
    struct RemoveStatus r = subtree_splayed_remove(me->root, key);
    if (r.removed == NULL) {
        return false;
    }
    me->root = r.new_child;
    --me->cardinality;
    free(r.removed);
    return true;
}
