/*
 * libdict -- weight-balanced tree implementation.
 * cf. [Gonnet 1984], [Nievergelt and Reingold 1973]
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

#include "wb_tree.h"

#include <limits.h>
#include "dict_private.h"
#include "tree_common.h"

/* A tree BB[alpha] is said to be of weighted balance alpha if every node in
 * the tree has a balance p(n) such that alpha <= p(n) <= 1 - alpha. The
 * balance of a node is defined as the number of nodes in its left subtree
 * divided by the number of nodes in either subtree. The weight of a node is
 * defined as the number of external nodes in its subtrees.
 *
 * Legal values for alpha are 0 <= alpha <= 1/2. BB[0] is a normal, unbalanced
 * binary tree, and BB[1/2] includes only completely balanced binary search
 * trees of 2^height - 1 nodes. A higher value of alpha specifies a more
 * stringent balance requirement. Values for alpha in the range 2/11 <= alpha
 * <= 1 - sqrt(2)/2 are interesting because a tree can be brought back into
 * weighted balance after an insertion or deletion using at most one rotation
 * per level (thus the number of rotations after insertion or deletion is
 * O(lg N)).
 *
 * These are the parameters for alpha = 1 - sqrt(2)/2 == .292893, as
 * recommended in [Gonnet 1984]. */

/* TODO: approximate these constants with integer fractions and eliminate
 * floating point arithmetic throughout.  */
#define ALPHA_0		.292893f	/* 1 - sqrt(2)/2		*/
#define ALPHA_1		.707106f	/* sqrt(2)/2			*/
#define ALPHA_2		.414213f	/* sqrt(2) - 1			*/
#define ALPHA_3		.585786f	/* 2 - sqrt(2)			*/

typedef struct wb_node wb_node;
struct wb_node {
    TREE_NODE_FIELDS(wb_node);
    uint32_t			weight;
};

#define WEIGHT(n)	((n) ? (n)->weight : 1)

struct wb_tree {
    TREE_FIELDS(wb_node);
};

struct wb_itor {
    wb_tree*			tree;
    wb_node*			node;
};

static dict_vtable wb_tree_vtable = {
    (dict_inew_func)		wb_dict_itor_new,
    (dict_dfree_func)		tree_free,
    (dict_insert_func)		wb_tree_insert,
    (dict_probe_func)		wb_tree_probe,
    (dict_search_func)		tree_search,
    (dict_remove_func)		wb_tree_remove,
    (dict_clear_func)		tree_clear,
    (dict_traverse_func)	tree_traverse,
    (dict_count_func)		wb_tree_count
};

static itor_vtable wb_tree_itor_vtable = {
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
    (dict_iremove_func)		NULL,/* wb_itor_remove not implemented yet */
    (dict_icompare_func)	NULL /* wb_itor_compare not implemented yet */
};

static void	rot_left(wb_tree *tree, wb_node *node);
static void	rot_right(wb_tree *tree, wb_node *node);
static size_t	node_height(const wb_node *node);
static size_t	node_mheight(const wb_node *node);
static size_t	node_pathlen(const wb_node *node, size_t level);
static wb_node*	node_new(void *key, void *datum);

wb_tree *
wb_tree_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
    wb_tree *tree = MALLOC(sizeof(*tree));
    if (tree) {
	tree->root = NULL;
	tree->count = 0;
	tree->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	tree->del_func = del_func;
    }
    return tree;
}

dict *
wb_dict_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
    dict *dct = MALLOC(sizeof(*dct));
    if (dct) {
	if (!(dct->_object = wb_tree_new(cmp_func, del_func))) {
	    FREE(dct);
	    return NULL;
	}
	dct->_vtable = &wb_tree_vtable;
    }
    return dct;
}

size_t
wb_tree_free(wb_tree *tree)
{
    ASSERT(tree != NULL);

    size_t count = tree_clear(tree);
    FREE(tree);
    return count;
}

void *
wb_tree_search(wb_tree *tree, const void *key)
{
    ASSERT(tree != NULL);

    return tree_search(tree, key);
}

int
wb_tree_insert(wb_tree *tree, void *key, void *datum, bool overwrite)
{
    int cmp = 0;

    ASSERT(tree != NULL);

    wb_node *node = tree->root, *parent = NULL;
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
	parent = node->parent;
	++node->weight;
	float wbal = WEIGHT(node->llink) / (float)node->weight;
	if (wbal < ALPHA_0) {
	    ASSERT(node->rlink != NULL);
	    wbal = WEIGHT(node->rlink->llink) / (float)node->rlink->weight;
	    if (wbal < ALPHA_3) {		/* LL */
		rot_left(tree, node);
	    } else {				/* RL */
		rot_right(tree, node->rlink);
		rot_left(tree, node);
	    }
	} else if (wbal > ALPHA_1) {
	    ASSERT(node->llink != NULL);
	    wbal = WEIGHT(node->llink->llink) / (float)node->llink->weight;
	    if (wbal > ALPHA_2) {		/* RR */
		rot_right(tree, node);
	    } else {				/* LR */
		rot_left(tree, node->llink);
		rot_right(tree, node);
	    }
	}
    }
    ++tree->count;
    return 0;
}

int
wb_tree_probe(wb_tree *tree, void *key, void **datum)
{
    ASSERT(tree != NULL);

    int cmp = 0;
    wb_node *node = tree->root, *parent = NULL;
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
    }

    if (!(node = node_new(key, *datum)))
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
	parent = node->parent;
	++node->weight;
	float wbal = WEIGHT(node->llink) / (float)node->weight;
	if (wbal < ALPHA_0) {
	    ASSERT(node->rlink != NULL);
	    wbal = WEIGHT(node->rlink->llink) / (float)node->rlink->weight;
	    if (wbal < ALPHA_3) {
		rot_left(tree, node);
	    } else {
		rot_right(tree, node->rlink);
		rot_left(tree, node);
	    }
	} else if (wbal > ALPHA_1) {
	    ASSERT(node->llink != NULL);
	    wbal = WEIGHT(node->llink->llink) / (float)node->llink->weight;
	    if (wbal > ALPHA_2) {
		rot_right(tree, node);
	    } else {
		rot_left(tree, node->llink);
		rot_right(tree, node);
	    }
	}
    }
    ++tree->count;
    return 1;
}

bool
wb_tree_remove(wb_tree *tree, const void *key)
{
    ASSERT(tree != NULL);
    ASSERT(key != NULL);

    wb_node *node = tree->root;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0) {
	    node = node->llink;
	    continue;
	} else if (cmp) {
	    node = node->rlink;
	    continue;
	}
	if (!node->llink || !node->rlink) {
	    /* Splice in the successor, if any. */
	    wb_node *out = node->llink ? node->llink : node->rlink;
	    if (out)
		out->parent = node->parent;
	    if (node->parent) {
		if (node->parent->llink == node)
		    node->parent->llink = out;
		else
		    node->parent->rlink = out;
	    } else {
		ASSERT(tree->root == node);
		tree->root = out;
	    }
	    out = node->parent;
	    if (tree->del_func)
		tree->del_func(node->key, node->datum);
	    FREE(node);
	    --tree->count;
	    /* Now move up the tree, decrementing weights. */
	    while (out) {
		--out->weight;
		out = out->parent;
	    }
	    return true;
	}
	/* If we get here, both llink and rlink are not NULL. */
	if (node->llink->weight > node->rlink->weight) {
	    wb_node *llink = node->llink;
	    if (WEIGHT(llink->llink) < WEIGHT(llink->rlink)) {
		rot_left(tree, llink);
		llink = node->llink;
	    }
	    rot_right(tree, node);
	    node = llink->rlink;
	} else {
	    wb_node *rlink = node->rlink;
	    if (WEIGHT(rlink->rlink) < WEIGHT(rlink->llink)) {
		rot_right(tree, rlink);
		rlink = node->rlink;
	    }
	    rot_left(tree, node);
	    node = rlink->llink;
	}
    }
    return false;
}

size_t
wb_tree_clear(wb_tree *tree)
{
    ASSERT(tree != NULL);

    return tree_clear(tree);
}

const void *
wb_tree_min(const wb_tree *tree)
{
    ASSERT(tree != NULL);

    if (!tree->root)
	return NULL;

    const wb_node *node = tree->root;
    while (node->llink)
	node = node->llink;
    return node->key;
}

const void *
wb_tree_max(const wb_tree *tree)
{
    ASSERT(tree != NULL);

    if (!tree->root)
	return NULL;

    const wb_node *node = tree->root;
    while (node->rlink)
	node = node->rlink;
    return node->key;
}

size_t
wb_tree_traverse(wb_tree *tree, dict_visit_func visit)
{
    ASSERT(tree != NULL);

    if (!tree->root)
	return 0;
    size_t count = 0;
    wb_node *node = tree_node_min(tree->root);
    while (node) {
	++count;
	if (!visit(node->key, node->datum))
	    break;
	node = tree_node_next(node);
    }
    return count;
}

size_t
wb_tree_count(const wb_tree *tree)
{
    ASSERT(tree != NULL);

    return tree->count;
}

size_t
wb_tree_height(const wb_tree *tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_height(tree->root) : 0;
}

size_t
wb_tree_mheight(const wb_tree *tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_mheight(tree->root) : 0;
}

size_t
wb_tree_pathlen(const wb_tree *tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_pathlen(tree->root, 1) : 0;
}

static wb_node *
node_new(void *key, void *datum)
{
    wb_node *node = MALLOC(sizeof(*node));
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
node_height(const wb_node *node)
{
    ASSERT(node != NULL);

    size_t l = node->llink ? node_height(node->llink) + 1 : 0;
    size_t r = node->rlink ? node_height(node->rlink) + 1 : 0;
    return MAX(l, r);
}

static size_t
node_mheight(const wb_node *node)
{
    ASSERT(node != NULL);

    size_t l = node->llink ? node_mheight(node->llink) + 1 : 0;
    size_t r = node->rlink ? node_mheight(node->rlink) + 1 : 0;
    return MIN(l, r);
}

static size_t
node_pathlen(const wb_node *node, size_t level)
{
    size_t n = 0;

    ASSERT(node != NULL);

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
 * Only the weights of B and B's right child need to be readjusted.
 */
static void
rot_left(wb_tree *tree, wb_node *node)
{
    ASSERT(tree != NULL);
    ASSERT(node != NULL);
    ASSERT(node->rlink != NULL);

    wb_node *rlink = node->rlink;
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
rot_right(wb_tree *tree, wb_node *node)
{
    ASSERT(tree != NULL);
    ASSERT(node != NULL);
    ASSERT(node->llink != NULL);

    wb_node *llink = node->llink;
    tree_node_rot_right(tree, node);

    node->weight = WEIGHT(node->llink) + WEIGHT(node->rlink);
    llink->weight = WEIGHT(llink->llink) + node->weight;
}

wb_itor *
wb_itor_new(wb_tree *tree)
{
    ASSERT(tree != NULL);

    wb_itor *itor = MALLOC(sizeof(*itor));
    if (itor) {
	itor->tree = tree;
	itor->node = NULL;
    }
    return itor;
}

dict_itor *
wb_dict_itor_new(wb_tree *tree)
{
    ASSERT(tree != NULL);
    dict_itor *itor = MALLOC(sizeof(*itor));
    if (itor) {
	if (!(itor->_itor = wb_itor_new(tree))) {
	    FREE(itor);
	    return NULL;
	}
	itor->_vtable = &wb_tree_itor_vtable;
    }
    return itor;
}

void
wb_itor_free(wb_itor *itor)
{
	ASSERT(itor != NULL);

	FREE(itor);
}

bool
wb_itor_valid(const wb_itor *itor)
{
    ASSERT(itor != NULL);

    return itor->node != NULL;
}

void
wb_itor_invalidate(wb_itor *itor)
{
    ASSERT(itor != NULL);

    itor->node = NULL;
}

bool
wb_itor_next(wb_itor *itor)
{
    ASSERT(itor != NULL);

    if (!itor->node) {
	return wb_itor_first(itor);
    } else {
	itor->node = tree_node_next(itor->node);
	return itor->node != NULL;
    }
}

bool
wb_itor_prev(wb_itor *itor)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	return wb_itor_last(itor);
    else {
	itor->node = tree_node_prev(itor->node);
	return itor->node != NULL;
    }
}

bool
wb_itor_nextn(wb_itor *itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!wb_itor_next(itor))
	    return false;
    return itor->node != NULL;
}

bool
wb_itor_prevn(wb_itor *itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!wb_itor_prev(itor))
	    return false;
    return itor->node != NULL;
}

bool
wb_itor_first(wb_itor *itor)
{
    ASSERT(itor != NULL);

    if (itor->tree->root) {
	itor->node = tree_node_min(itor->tree->root);
	return true;
    } else {
	itor->node = NULL;
	return false;
    }
}

bool
wb_itor_last(wb_itor *itor)
{
    ASSERT(itor != NULL);

    if (itor->tree->root) {
	itor->node = tree_node_max(itor->tree->root);
	return true;
    } else {
	itor->node = NULL;
	return false;
    }
}

bool
wb_itor_search(wb_itor *itor, const void *key)
{
    ASSERT(itor != NULL);

    for (wb_node *node = itor->tree->root; node;) {
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
wb_itor_key(const wb_itor *itor)
{
    ASSERT(itor != NULL);

    return itor->node ? itor->node->key : NULL;
}

void *
wb_itor_data(wb_itor *itor)
{
    ASSERT(itor != NULL);

    return itor->node ? itor->node->datum : NULL;
}

bool
wb_itor_set_data(wb_itor *itor, void *datum, void **old_datum)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	return false;

    if (old_datum)
	*old_datum = itor->node->datum;
    itor->node->datum = datum;
    return true;
}

static void
wb_node_verify(const wb_tree *tree, wb_node *node, wb_node *parent)
{
    if (!parent) {
	ASSERT(tree->root == node);
    }
    if (node) {
	ASSERT(node->parent == parent);
	wb_node_verify(tree, node->llink, node);
	wb_node_verify(tree, node->rlink, node);
    }
}

void
wb_tree_verify(const wb_tree *tree)
{
    ASSERT(tree != NULL);

    wb_node_verify(tree, tree->root, NULL);
}
