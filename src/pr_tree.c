/*
 * libdict -- internal path reduction tree implementation.
 * cf. [Gonnet 1983], [Gonnet 1984]
 *
 * Copyright (c) 2001-2011, Farooq Mela
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Farooq Mela.
 * 4. Neither the name of the Farooq Mela nor the
 *    names of contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY FAROOQ MELA ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL FAROOQ MELA BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pr_tree.h"

#include "dict_private.h"
#include "tree_common.h"

typedef struct pr_node pr_node;
struct pr_node {
    TREE_NODE_FIELDS(pr_node);
    unsigned			weight;
};

#define WEIGHT(n)	((n) ? (n)->weight : 1)

struct pr_tree {
    TREE_FIELDS(pr_node);
};

struct pr_itor {
    pr_tree*			tree;
    pr_node*			node;
};

static dict_vtable pr_tree_vtable = {
    (dict_inew_func)		pr_dict_itor_new,
    (dict_dfree_func)		tree_free,
    (dict_insert_func)		pr_tree_insert,
    (dict_probe_func)		pr_tree_probe,
    (dict_search_func)		tree_search,
    (dict_remove_func)		pr_tree_remove,
    (dict_clear_func)		tree_clear,
    (dict_traverse_func)	tree_traverse,
    (dict_count_func)		tree_count,
};

static itor_vtable pr_tree_itor_vtable = {
    (dict_ifree_func)		tree_iterator_free,
    (dict_valid_func)		tree_iterator_valid,
    (dict_invalidate_func)	tree_iterator_invalidate,
    (dict_next_func)		tree_iterator_next,
    (dict_prev_func)		tree_iterator_prev,
    (dict_nextn_func)		tree_iterator_next_n,
    (dict_prevn_func)		tree_iterator_prev_n,
    (dict_first_func)		tree_iterator_first,
    (dict_last_func)		tree_iterator_last,
    (dict_key_func)		tree_iterator_key,
    (dict_data_func)		tree_iterator_data,
    (dict_dataset_func)		tree_iterator_set_data,
    (dict_iremove_func)		NULL,/* pr_itor_remove not implemented yet */
    (dict_icompare_func)	NULL /* pr_itor_compare not implemented yet */
};

static void	fixup(pr_tree *tree, pr_node *node);
static void	rot_left(pr_tree *tree, pr_node *node);
static void	rot_right(pr_tree *tree, pr_node *node);
static size_t	node_height(const pr_node *node);
static size_t	node_mheight(const pr_node *node);
static size_t	node_pathlen(const pr_node *node, size_t level);
static pr_node*	node_new(void *key, void *datum);

pr_tree *
pr_tree_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
    pr_tree *tree = MALLOC(sizeof(*tree));
    if (tree) {
	tree->root = NULL;
	tree->count = 0;
	tree->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	tree->del_func = del_func;
    }
    return tree;
}

dict *
pr_dict_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
    dict *dct = MALLOC(sizeof(*dct));
    if (dct) {
	if (!(dct->_object = pr_tree_new(cmp_func, del_func))) {
	    FREE(dct);
	    return NULL;
	}
	dct->_vtable = &pr_tree_vtable;
    }
    return dct;
}

size_t
pr_tree_free(pr_tree *tree)
{
    ASSERT(tree != NULL);

    size_t count = pr_tree_clear(tree);
    FREE(tree);
    return count;
}

void *
pr_tree_search(pr_tree *tree, const void *key)
{
    ASSERT(tree != NULL);

    pr_node *node = tree->root;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp)
	    node = node->rlink;
	else
	    return node->datum;
    }

    return NULL;
}

static void
fixup(pr_tree *tree, pr_node *node)
{
    ASSERT(tree != NULL);
    ASSERT(node != NULL);

    /*
     * The internal path length of a tree is defined as the sum of levels of
     * all the tree's internal nodes. Path-reduction trees are similar to
     * weight-balanced trees, except that rotations are only made when they can
     * reduce the total internal path length of the tree. These particular
     * trees are in the class BB[1/3], but because of these restrictions their
     * performance is superior to BB[1/3] trees.
     *
     * Consider a node N.
     * A single left rotation is performed when
     * WEIGHT(n->llink) < WEIGHT(n->rlink->rlink)
     * A right-left rotation is performed when
     * WEIGHT(n->llink) < WEIGHT(n->rlink->llink)
     *
     * A single right rotation is performed when
     * WEIGHT(n->rlink) > WEIGHT(n->llink->llink)
     * A left-right rotation is performed when
     * WEIGHT(n->rlink) > WEIGHT(n->llink->rlink)
     *
     * Although the worst case number of rotations for a single insertion or
     * deletion is O(n), the amortized worst-case number of rotations is
     * .44042lg(n) + O(1) for insertion, and .42062lg(n) + O(1) for deletion.
     *
     * We use tail recursion to minimize the number of recursive calls. For
     * single rotations, no recursive call is made, and we tail recurse to
     * continue checking for out-of-balance conditions. For double, we make one
     * recursive call and then tail recurse.
     */
    unsigned wl, wr;
again:
    wl = WEIGHT(node->llink);
    wr = WEIGHT(node->rlink);
    if (wr > wl) {
	ASSERT(node->rlink != NULL);
	if (WEIGHT(node->rlink->rlink) > wl) {		/* LL */
	    rot_left(tree, node);
	    goto again;
	} else if (WEIGHT(node->rlink->llink) > wl) {	/* RL */
	    pr_node *temp = node->rlink;
	    rot_right(tree, temp);
	    rot_left(tree, node);
	    if (temp->rlink)
		fixup(tree, temp->rlink);
	    goto again;
	}
    } else if (wl > wr) {
	ASSERT(node->llink != NULL);
	if (WEIGHT(node->llink->llink) > wr) {		/* RR */
	    rot_right(tree, node);
	    goto again;
	} else if (WEIGHT(node->llink->rlink) > wr) {	/* LR */
	    pr_node *temp = node->llink;
	    rot_left(tree, temp);
	    rot_right(tree, node);
	    if (temp->llink)
		fixup(tree, temp->llink);
	    goto again;
	}
    }
}

int
pr_tree_insert(pr_tree *tree, void *key, void *datum, bool overwrite)
{
    ASSERT(tree != NULL);

    int cmp = 0;
    pr_node *node = tree->root, *parent = NULL;
    while (node) {
	cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    parent = node, node = node->llink;
	else if (cmp)
	    parent = node, node = node->rlink;
	else {
	    if (!overwrite)
		return 1;
	    if (tree->del_func)
		tree->del_func(node->key, node->datum);
	    node->key = key;
	    node->datum = datum;
	    return 0;
	}
    }

    if (!(node = node_new(key, datum)))
	return -1;
    if (!(node->parent = parent)) {
	ASSERT(tree->count == 0);
	tree->root = node;
	tree->count = 1;
	return 0;
    }
    if (cmp < 0)
	parent->llink = node;
    else
	parent->rlink = node;

    while ((node = parent) != NULL) {
	parent = parent->parent;
	node->weight++;
	fixup(tree, node);
    }

    tree->count++;
    return 0;
}

int
pr_tree_probe(pr_tree *tree, void *key, void **datum)
{
    ASSERT(tree != NULL);

    int cmp = 0;
    pr_node *node = tree->root, *parent = NULL;
    while (node) {
	cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    parent = node, node = node->llink;
	else if (cmp)
	    parent = node, node = node->rlink;
	else {
	    *datum = node->datum;
	    return 0;
	}
	parent = node;
	node = (cmp < 0) ? node->llink : node->rlink;
    }

    if (!(node = node_new(key, *datum)))
	return -1;
    if (!(node->parent = parent)) {
	ASSERT(tree->count == 0);
	tree->root = node;
	tree->count = 1;
	return 1;
    }

    if (cmp < 0)
	parent->llink = node;
    else
	parent->rlink = node;

    while ((node = parent) != NULL) {
	parent = parent->parent;
	node->weight++;
	fixup(tree, node);
    }

    tree->count++;
    return 1;
}

bool
pr_tree_remove(pr_tree *tree, const void *key)
{
    ASSERT(tree != NULL);
    ASSERT(key != NULL);

    pr_node *node = tree->root, *temp, *out = NULL;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0) {
	    node = node->llink;
	    continue;
	} else if (cmp) {
	    node = node->rlink;
	    continue;
	}
	if (!node->llink) {
	    out = node->rlink;
	    if (out)
		out->parent = node->parent;

	    if (node->parent) {
		if (node->parent->llink == node)
		    node->parent->llink = out;
		else
		    node->parent->rlink = out;
	    } else {
		tree->root = out;
	    }
	    temp = node->parent;

	    if (tree->del_func)
		tree->del_func(node->key, node->datum);
	    FREE(node);

	    for (; temp; temp = temp->parent)
		temp->weight--;
	    tree->count--;
	    return true;
	} else if (!node->rlink) {
	    out = node->llink;
	    if (out)
		out->parent = node->parent;
	    if (node->parent) {
		if (node->parent->llink == node)
		    node->parent->llink = out;
		else
		    node->parent->rlink = out;
	    } else {
		tree->root = out;
	    }
	    temp = node->parent;

	    if (tree->del_func)
		tree->del_func(node->key, node->datum);
	    FREE(node);

	    for (; temp; temp = temp->parent)
		temp->weight--;
	    tree->count--;
	    return true;
	} else if (WEIGHT(node->llink) > WEIGHT(node->rlink)) {
	    if (WEIGHT(node->llink->llink) < WEIGHT(node->llink->rlink))
		rot_left(tree, node->llink);
	    out = node->llink;
	    rot_right(tree, node);
	    node = out->rlink;
	} else {
	    if (WEIGHT(node->rlink->rlink) < WEIGHT(node->rlink->llink))
		rot_right(tree, node->rlink);
	    out = node->rlink;
	    rot_left(tree, node);
	    node = out->llink;
	}
    }
    return false;
}

size_t
pr_tree_clear(pr_tree *tree)
{
    ASSERT(tree != NULL);

    const size_t count = tree->count;
    pr_node *node = tree->root;
    while (node) {
	if (node->llink || node->rlink) {
	    node = node->llink ? node->llink : node->rlink;
	    continue;
	}

	if (tree->del_func)
	    tree->del_func(node->key, node->datum);

	pr_node *parent = node->parent;
	FREE(node);

	if (parent) {
	    if (parent->llink == node)
		parent->llink = NULL;
	    else
		parent->rlink = NULL;
	}
	node = parent;
    }

    tree->root = NULL;
    tree->count = 0;
    return count;
}

const void *
pr_tree_min(const pr_tree *tree)
{
    ASSERT(tree != NULL);

    if (!tree->root)
	return NULL;
    const pr_node *node = tree->root;
    for (; node->llink; node = node->llink)
	/* void */;
    return node->key;
}

const void *
pr_tree_max(const pr_tree *tree)
{
    ASSERT(tree != NULL);

    if (!tree->root)
	return NULL;
    const pr_node *node = tree->root;
    for (; node->rlink; node = node->rlink)
	/* void */;
    return node->key;
}

size_t
pr_tree_traverse(pr_tree *tree, dict_visit_func visit)
{
    ASSERT(tree != NULL);

    return tree_traverse(tree, visit);
}

size_t
pr_tree_count(const pr_tree *tree)
{
    ASSERT(tree != NULL);

    return tree_count(tree);
}

size_t
pr_tree_height(const pr_tree *tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_height(tree->root) : 0;
}

size_t
pr_tree_mheight(const pr_tree *tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_mheight(tree->root) : 0;
}

size_t
pr_tree_pathlen(const pr_tree *tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_pathlen(tree->root, 1) : 0;
}

static pr_node *
node_new(void *key, void *datum)
{
    pr_node *node = MALLOC(sizeof(*node));
    if (node) {
	node->key = key;
	node->datum = datum;
	node->parent = NULL;
	node->llink = NULL;
	node->rlink = NULL;
	node->weight = 2;
    }
    return node;
}

static size_t
node_height(const pr_node *node)
{
    ASSERT(node != NULL);

    size_t l = node->llink ? node_height(node->llink) + 1 : 0;
    size_t r = node->rlink ? node_height(node->rlink) + 1 : 0;
    return MAX(l, r);
}

static size_t
node_mheight(const pr_node *node)
{
    ASSERT(node != NULL);

    size_t l = node->llink ? node_mheight(node->llink) + 1 : 0;
    size_t r = node->rlink ? node_mheight(node->rlink) + 1 : 0;
    return MIN(l, r);
}

static size_t
node_pathlen(const pr_node *node, size_t level)
{
    ASSERT(node != NULL);

    size_t n = 0;
    if (node->llink)
	n += level + node_pathlen(node->llink, level + 1);
    if (node->rlink)
	n += level + node_pathlen(node->rlink, level + 1);
    return n;
}

/*
 * rot_left(T, B):
 *
 *     /             /
 *    B             D
 *   / \           / \
 *  A   D   ==>   B   E
 *     / \       / \
 *    C   E     A   C
 *
 * Only the weights of B and B's right child to be readjusted.
 */
static void
rot_left(pr_tree *tree, pr_node	*node)
{
    ASSERT(tree != NULL);
    ASSERT(node != NULL);
    ASSERT(node->rlink != NULL);

    pr_node *rlink = node->rlink;
    tree_node_rot_left(tree, node);

    node->weight = WEIGHT(node->llink) + WEIGHT(node->rlink);
    rlink->weight = node->weight + WEIGHT(rlink->rlink);
}

/*
 * rot_right(T, D):
 *
 *       /           /
 *      D           B
 *     / \         / \
 *    B   E  ==>  A   D
 *   / \             / \
 *  A   C           C   E
 *
 * Only the weights of D and D's left child need to be readjusted.
 */
static void
rot_right(pr_tree *tree, pr_node *node)
{
    ASSERT(tree != NULL);
    ASSERT(node != NULL);
    ASSERT(node->llink != NULL);

    pr_node *llink = node->llink;
    tree_node_rot_right(tree, node);

    node->weight = WEIGHT(node->llink) + WEIGHT(node->rlink);
    llink->weight = WEIGHT(llink->llink) + node->weight;
}

pr_itor *
pr_itor_new(pr_tree *tree)
{
    ASSERT(tree != NULL);

    pr_itor *itor = MALLOC(sizeof(*itor));
    if (itor) {
	itor->tree = tree;
	itor->node = NULL;
    }
    return itor;
}

dict_itor *
pr_dict_itor_new(pr_tree *tree)
{

    ASSERT(tree != NULL);

    dict_itor *itor = MALLOC(sizeof(*itor));
    if (itor) {
	if (!(itor->_itor = pr_itor_new(tree))) {
	    FREE(itor);
	    return NULL;
	}
	itor->_vtable = &pr_tree_itor_vtable;
    }
    return itor;
}

void
pr_itor_free(pr_itor *itor)
{
    ASSERT(itor != NULL);

    FREE(itor);
}

bool
pr_itor_valid(const pr_itor *itor)
{
    ASSERT(itor != NULL);

    return itor->node != NULL;
}

void
pr_itor_invalidate(pr_itor *itor)
{
    ASSERT(itor != NULL);

    itor->node = NULL;
}

bool
pr_itor_next(pr_itor *itor)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	pr_itor_first(itor);
    else
	itor->node = tree_node_next(itor->node);
    return itor->node != NULL;
}

bool
pr_itor_prev(pr_itor *itor)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	pr_itor_last(itor);
    else
	itor->node = tree_node_prev(itor->node);
    return itor->node != NULL;
}

bool
pr_itor_nextn(pr_itor *itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!pr_itor_next(itor))
	    return false;
    return itor->node != NULL;
}

bool
pr_itor_prevn(pr_itor *itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!pr_itor_prev(itor))
	    return false;
    return itor->node != NULL;
}

bool
pr_itor_first(pr_itor *itor)
{
    ASSERT(itor != NULL);

    if (!itor->tree->root)
	itor->node = NULL;
    else
	itor->node = tree_node_min(itor->tree->root);
    return itor->node != NULL;
}

bool
pr_itor_last(pr_itor *itor)
{
    ASSERT(itor != NULL);

    if (!itor->tree->root)
	itor->node = NULL;
    else
	itor->node = tree_node_max(itor->tree->root);
    return itor->node != NULL;
}

bool
pr_itor_search(pr_itor *itor, const void *key)
{
    ASSERT(itor != NULL);

    pr_node *node = itor->tree->root;
    while (node) {
	int cmp = itor->tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp)
	    node = node->rlink;
	else {
	    itor->node = node;
	    return true;
	}
    }
    itor->node = NULL;
    return false;
}

const void *
pr_itor_key(const pr_itor *itor)
{
    ASSERT(itor != NULL);

    return itor->node ? itor->node->key : NULL;
}

void *
pr_itor_data(pr_itor *itor)
{
    ASSERT(itor != NULL);

    return itor->node ? itor->node->datum : NULL;
}

bool
pr_itor_set_data(pr_itor *itor, void *datum, void **old_datum)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	return false;

    if (old_datum)
	*old_datum = itor->node->datum;
    itor->node->datum = datum;
    return true;
}
