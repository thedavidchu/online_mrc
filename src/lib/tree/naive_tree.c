#include <assert.h>
#include <stdlib.h>

#include "tree/naive_tree.h"

struct NaiveSubtree {
    KeyType key;
    size_t cardinality;
    struct NaiveSubtree *left_subtree;
    struct NaiveSubtree *right_subtree;
};

struct NaiveTree {
    struct NaiveSubtree *root;
    size_t cardinality;
};

struct RemoveStatus {
    struct NaiveSubtree *removed;
    struct NaiveSubtree *new_child;
};

struct NaiveSubtree *
subtree_new(KeyType key)
{
    struct NaiveSubtree *subtree = (struct NaiveSubtree *)malloc(sizeof(struct NaiveSubtree));
    if (subtree == NULL) {
        assert(0 && "OOM error!");
        return NULL;
    }
    subtree->key = key;
    subtree->cardinality = 1;
    subtree->left_subtree = NULL;
    subtree->right_subtree = NULL;
    return subtree;
}

struct NaiveTree *
tree_new()
{
    struct NaiveTree *r = (struct NaiveTree *)malloc(sizeof(struct NaiveTree));
    if (r == NULL) {
        assert(0 && "OOM error!");
        return NULL;
    }
    r->cardinality = 0;
    r->root = NULL;
    return r;
}

size_t
tree_cardinality(struct NaiveTree *me)
{
    return me->cardinality;
}

static bool
subtree_insert(struct NaiveSubtree *me, KeyType key)
{
    struct NaiveSubtree **next_subtree;
    if (me == NULL) {
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
            return false;   // OOM error!
        }
        return true;
    } else {
        bool r = subtree_insert(*next_subtree, key);
        if (r) {
            ++me->cardinality;
        }
        return r;
    }
}

bool
tree_insert(struct NaiveTree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    }
    bool r = subtree_insert(me->root,key);
    if (r) {
        ++me->cardinality;
    }
    return r;
}

static bool
subtree_search(struct NaiveSubtree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    } else if (key < me->key) {
        return subtree_search(me->left_subtree, key);
    } else if (me->key < key) {
        return subtree_search(me->right_subtree, key);
    } else {
        return true;
    }
}

bool
tree_search(struct NaiveTree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    }
    return subtree_search(me->root, key);
}

static struct RemoveStatus
subtree_pop_minimum(struct NaiveSubtree *me)
{
    if (me == NULL) {
        return (struct RemoveStatus) {.removed = NULL, .new_child = NULL};
    } else if (me->left_subtree == NULL) {
        return (struct RemoveStatus) {.removed = me, .new_child = me->right_subtree};
    } else {
        struct RemoveStatus r = subtree_pop_minimum(me->left_subtree);
        if (r.new_child != me->left_subtree) {
            me->left_subtree = r.new_child;
        }
        return r;
    }
}

static struct RemoveStatus
subtree_remove(struct NaiveSubtree *me, KeyType key)
{
    if (me == NULL) {
        return (struct RemoveStatus) {.removed = false, .new_child = NULL};
    } else if (key < me->key) {
        struct RemoveStatus r = subtree_remove(me->left_subtree, key);
        if (r.removed) {
            --me->cardinality;
        }
        me->left_subtree = r.new_child;
        r.new_child = me;
        return r;
    } else if (me->key < key) {
        struct RemoveStatus r = subtree_remove(me->right_subtree, key);
        if (r.removed) {
            --me->cardinality;
        }
        me->right_subtree = r.new_child;
        r.new_child = me;
        r.new_child = me;
        return r;
    } else {
        if (me->left_subtree == NULL && me->right_subtree == NULL) {
            // Is a leaf
            return (struct RemoveStatus) {.removed = me, .new_child = NULL};
        } else if (me->left_subtree != NULL) {
            // Single child
            struct NaiveSubtree *single_child = me->left_subtree;
            return (struct RemoveStatus) {.removed = me, .new_child = single_child};
        } else if (me->right_subtree != NULL) {
            // Single child
            struct NaiveSubtree *single_child = me->right_subtree;
            return (struct RemoveStatus) {.removed = me, .new_child = single_child};
        } else {
            // Double child => recursively replace with successor (arbitrarily
            // chose the successor rather than the predecessor).
            struct RemoveStatus r = subtree_pop_minimum(me->right_subtree);
            return r;
        }
    }
}

bool
tree_remove(struct NaiveTree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    }
    struct RemoveStatus r = subtree_remove(me->root, key);
    if (r.removed == NULL) {
        return false;
    }
    free(r.removed);
    return true;
}
