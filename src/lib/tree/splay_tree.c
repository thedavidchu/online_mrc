#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "tree/basic_tree.h"
#include "tree/splay_tree.h"

/// NOTE    Copied from Sleator's original Splay tree implementation (modified by Parda guy)
/// This is the comparison.
/// Returns <0 if i<j, =0 if i=j, and >0 if i>j
#define compare(i, j) ((i) - (j))

/// NOTE    Copied from Sleator's original Splay tree implementation (modified by Parda guy)
/// This macro returns the size of a node.  Unlike "x->size",
/// it works even if x=NULL.  The test could be avoided by using
/// a special version of NULL which was a real node with size 0.
#define node_size(x) (((x) == NULL) ? 0 : ((x)->cardinality))

/// NOTE    Copied from Sleator's original Splay tree implementation (modified by Parda guy)
/// Splay using the key i (which may or may not be in the tree.)
/// The starting root is t, and the tree used is defined by rat
/// size fields are maintained
static struct Subtree *
sleator_splay(struct Subtree *t, KeyType i)
{
    Subtree N, *l, *r, *y;
    KeyType comp, l_size, r_size;
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
                t->cardinality = node_size(t->left_subtree) + node_size(t->right_subtree) + 1;
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
                t->right_subtree = y->left;
                y->left_subtree = t;
                t->cardinality = node_size(t->left_subtree) + node_size(t->right_subtree) + 1;
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
    l_size += node_size(t->left_subtree);  /* Now l_size and r_size are the sizes of */
    r_size += node_size(t->right_subtree); /* the left and right trees we just built.*/
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

bool
tree__splayed_insert(struct Tree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    }
    if (me->root == NULL) {
        me->root = subtree__new(key);
        if (me->root == NULL) {
            return false;
        }
        ++me->cardinality;
        return true;
    }
    struct Subtree *new_root = sleator_splay(me, key);
    if (new_root == NULL) {
        // Error!
        assert(0 && "impossible!");
        return false;
    } else if (new_root->key == key) {
        // Found it in the tree!
        me->root = new_root;
        return false;
    } else if (new_root->left_subtree == NULL) {
        new_root->left_subtree = subtree__new(key);
        if (new_root->left_subtree == NULL) {
            return false;
        }
        ++new_root->cardinality;
        me->root = new_root;
    } else if (new_root->right_subtree == NULL) {
        new_root->right_subtree = subtree__new(key);
        if (new_root->right_subtree == NULL) {
            return false;
        }
        ++new_root->cardinality;
        me->root = new_root;
    } else {
        // Splay didn't do its job!
        return false;
    }
}

static struct RemoveStatus
subtree__pop_minimum(struct Subtree *me)
{
    if (me == NULL) {
        // NOTE This should only be called if this function is originally called
        //      with a NULL argument. No recursive call should pass NULL.
        return (struct RemoveStatus){.removed = NULL, .new_child = NULL};
    } else if (me->left_subtree == NULL) {
        return (struct RemoveStatus){.removed = me, .new_child = me->right_subtree};
    } else {
        struct RemoveStatus r = subtree__pop_minimum(me->left_subtree);
        assert(r.removed != NULL && "should have popped an item from the non-empty subtree");
        --me->cardinality;
        me->left_subtree = r.new_child;
        r.new_child = me;
        return r;
    }
}

static struct RemoveStatus
subtree__splayed_remove(struct Subtree *me, KeyType key)
{
    if (me == NULL) {
        return (struct RemoveStatus){.removed = false, .new_child = NULL};
    } else if (key < me->key) {
        struct RemoveStatus r = subtree__splayed_remove(me->left_subtree, key);
        if (r.removed) {
            --me->cardinality;
        }
        // NOTE Automatically overwriting the rather than checking may cause the
        //      cache line to be needlessly dirtied.
        me->left_subtree = r.new_child;
        r.new_child = me;
        return r;
    } else if (me->key < key) {
        struct RemoveStatus r = subtree__splayed_remove(me->right_subtree, key);
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
            struct Subtree *single_child = me->right_subtree;
            return (struct RemoveStatus){.removed = me, .new_child = single_child};
        } else if (me->right_subtree == NULL) {
            // Single child
            struct Subtree *single_child = me->left_subtree;
            return (struct RemoveStatus){.removed = me, .new_child = single_child};
        } else {
            // Double child => recursively replace with successor (arbitrarily
            // chose the successor rather than the predecessor).
            struct RemoveStatus r = subtree__pop_minimum(me->right_subtree);
            struct Subtree *replacement_node = r.removed;
            replacement_node->cardinality = me->cardinality - 1;
            replacement_node->left_subtree = me->left_subtree;
            replacement_node->right_subtree = r.new_child;
            return (struct RemoveStatus){.removed = me, .new_child = replacement_node};
        }
    }
}

bool
tree__remove(struct Tree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    }
    struct RemoveStatus r = subtree__splayed_remove(me->root, key);
    if (r.removed == NULL) {
        return false;
    }
    me->root = r.new_child;
    --me->cardinality;
    free(r.removed);
    return true;
}
