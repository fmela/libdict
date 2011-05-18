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

#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#include "wb_tree.h"
#include "dict_private.h"

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

#define ALPHA_0		.292893f	/* 1 - sqrt(2)/2		*/
#define ALPHA_1		.707106f	/* sqrt(2)/2			*/
#define ALPHA_2		.414213f	/* sqrt(2) - 1			*/
#define ALPHA_3		.585786f	/* 2 - sqrt(2)			*/

typedef struct wb_node wb_node;
struct wb_node {
	void*				key;
	void*				datum;
	wb_node*			parent;
	wb_node*			llink;
	wb_node*			rlink;
	uint32_t			weight;
};

#define WEIGHT(n)	((n) ? (n)->weight : 1)
#define REWEIGH(n)	(n)->weight = WEIGHT((n)->llink) + WEIGHT((n)->rlink)

struct wb_tree {
	wb_node*			root;
	size_t				count;
	dict_compare_func	cmp_func;
	dict_delete_func	del_func;
};

struct wb_itor {
	wb_tree*			tree;
	wb_node*			node;
};

static dict_vtable wb_tree_vtable = {
	(dict_inew_func)		wb_dict_itor_new,
	(dict_dfree_func)		wb_tree_free,
	(dict_insert_func)		wb_tree_insert,
	(dict_probe_func)		wb_tree_probe,
	(dict_search_func)		wb_tree_search,
	(dict_csearch_func)		wb_tree_search,
	(dict_remove_func)		wb_tree_remove,
	(dict_clear_func)		wb_tree_clear,
	(dict_traverse_func)	wb_tree_traverse,
	(dict_count_func)		wb_tree_count
};

static itor_vtable wb_tree_itor_vtable = {
	(dict_ifree_func)		wb_itor_free,
	(dict_valid_func)		wb_itor_valid,
	(dict_invalidate_func)	wb_itor_invalidate,
	(dict_next_func)		wb_itor_next,
	(dict_prev_func)		wb_itor_prev,
	(dict_nextn_func)		wb_itor_nextn,
	(dict_prevn_func)		wb_itor_prevn,
	(dict_first_func)		wb_itor_first,
	(dict_last_func)		wb_itor_last,
	(dict_key_func)			wb_itor_key,
	(dict_data_func)		wb_itor_data,
	(dict_cdata_func)		wb_itor_cdata,
	(dict_dataset_func)		wb_itor_set_data,
	(dict_iremove_func)		NULL,/* wb_itor_remove not implemented yet */
	(dict_icompare_func)	NULL /* wb_itor_compare not implemented yet */
};

static void		rot_left(wb_tree *tree, wb_node *node);
static void		rot_right(wb_tree *tree, wb_node *node);
static size_t	node_height(const wb_node *node);
static size_t	node_mheight(const wb_node *node);
static uint64_t	node_pathlen(const wb_node *node, uint64_t level);
static wb_node*	node_new(void *key, void *datum);
static wb_node*	node_min(wb_node *node);
static wb_node*	node_max(wb_node *node);
static wb_node*	node_next(wb_node *node);
static wb_node*	node_prev(wb_node *node);

wb_tree *
wb_tree_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
	wb_tree *tree;

	if ((tree = MALLOC(sizeof(*tree))) == NULL)
		return NULL;

	tree->root = NULL;
	tree->count = 0;
	tree->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	tree->del_func = del_func;

	return tree;
}

dict *
wb_dict_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
	dict *dct;

	if ((dct = MALLOC(sizeof(*dct))) == NULL)
		return NULL;

	if ((dct->_object = wb_tree_new(cmp_func, del_func)) == NULL) {
		FREE(dct);
		return NULL;
	}

	dct->_vtable = &wb_tree_vtable;

	return dct;
}

size_t
wb_tree_free(wb_tree *tree)
{
	size_t count = 0;

	ASSERT(tree != NULL);

	if (tree->root)
		count = wb_tree_clear(tree);

	FREE(tree);

	return count;
}

void *
wb_tree_search(wb_tree *tree, const void *key)
{
	int cmp;
	wb_node *node;

	ASSERT(tree != NULL);

	node = tree->root;
	while (node) {
		cmp = tree->cmp_func(key, node->key);
		if (cmp < 0)
			node = node->llink;
		else if (cmp > 0)
			node = node->rlink;
		else
			return node->datum;
	}

	return NULL;
}

int
wb_tree_insert(wb_tree *tree, void *key, void *datum, int overwrite)
{
	int cmp = 0;
	wb_node *node, *parent = NULL;
	float wbal;

	ASSERT(tree != NULL);

	node = tree->root;
	while (node) {
		cmp = tree->cmp_func(key, node->key);
		if (cmp < 0)
			parent = node, node = node->llink;
		else if (cmp > 0)
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

	if ((node = node_new(key, datum)) == NULL)
		return -1;
	if ((node->parent = parent) == NULL) {
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
		node->weight++;
		wbal = WEIGHT(node->llink) / (float)node->weight;
		if (wbal < ALPHA_0) {
			ASSERT(node->rlink != NULL);
			wbal = WEIGHT(node->rlink->llink) / (float)node->rlink->weight;
			if (wbal < ALPHA_3) {		/* LL */
				rot_left(tree, node);
			} else {					/* RL */
				rot_right(tree, node->rlink);
				rot_left(tree, node);
			}
		} else if (wbal > ALPHA_1) {
			ASSERT(node->llink != NULL);
			wbal = WEIGHT(node->llink->llink) / (float)node->llink->weight;
			if (wbal > ALPHA_2) {		/* RR */
				rot_right(tree, node);
			} else {					/* LR */
				rot_left(tree, node->llink);
				rot_right(tree, node);
			}
		}
	}
	tree->count++;
	return 0;
}

int
wb_tree_probe(wb_tree *tree, void *key, void **datum)
{
	int cmp = 0;
	wb_node *node, *parent = NULL;
	float wbal;

	ASSERT(tree != NULL);

	node = tree->root;
	while (node) {
		cmp = tree->cmp_func(key, node->key);
		if (cmp < 0)
			parent = node, node = node->llink;
		else if (cmp > 0)
			parent = node, node = node->rlink;
		else {
			*datum = node->datum;
			return 0;
		}
	}

	if ((node = node_new(key, *datum)) == NULL)
		return -1;
	if ((node->parent = parent) == NULL) {
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
		node->weight++;
		wbal = WEIGHT(node->llink) / (float)node->weight;
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
	tree->count++;
	return 1;
}

int
wb_tree_remove(wb_tree *tree, const void *key)
{
	int cmp;
	wb_node *node;

	ASSERT(tree != NULL);
	ASSERT(key != NULL);

	node = tree->root;
	while (node) {
		cmp = tree->cmp_func(key, node->key);
		if (cmp) {
			node = cmp < 0 ? node->llink : node->rlink;
			continue;
		}
		if (node->llink == NULL || node->rlink == NULL) {
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
			return 0;
		}
		/* If we get here, both llink and rlink are not NULL. */
		if (node->llink->weight > node->rlink->weight) {
			wb_node *llink = node->llink;
			if (WEIGHT(llink->llink) < WEIGHT(llink->rlink))
				rot_left(tree, llink);
			rot_right(tree, node);
			node = llink->rlink;
		} else {
			wb_node *rlink = node->rlink;
			if (WEIGHT(rlink->rlink) < WEIGHT(rlink->llink))
				rot_right(tree, rlink);
			rot_left(tree, node);
			node = rlink->llink;
		}
	}
	return -1;
}

size_t
wb_tree_clear(wb_tree *tree)
{
	wb_node *node, *parent;
	size_t count = 0;

	ASSERT(tree != NULL);

	count = tree->count;
	node = tree->root;

	while (node) {
		if (node->llink || node->rlink) {
			node = node->llink ? node->llink : node->rlink;
			continue;
		}

		if (tree->del_func)
			tree->del_func(node->key, node->datum);

		parent = node->parent;
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
wb_tree_min(const wb_tree *tree)
{
	const wb_node *node;

	ASSERT(tree != NULL);

	if ((node = tree->root) == NULL)
		return NULL;

	for (; node->llink; node = node->llink)
		/* void */;
	return node->key;
}

const void *
wb_tree_max(const wb_tree *tree)
{
	const wb_node *node;

	ASSERT(tree != NULL);

	if (tree->root == NULL)
		return NULL;

	for (node = tree->root; node->rlink; node = node->rlink)
		/* void */;
	return node->key;
}

size_t
wb_tree_traverse(wb_tree *tree, dict_visit_func visit)
{
	wb_node *node;
	size_t count = 0;

	ASSERT(tree != NULL);

	if (tree->root == NULL)
		return 0;

	for (node = node_min(tree->root); node; node = node_next(node)) {
		++count;
		if (!visit(node->key, node->datum))
			break;
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

uint64_t
wb_tree_pathlen(const wb_tree *tree)
{
	ASSERT(tree != NULL);

	return tree->root ? node_pathlen(tree->root, 1) : 0;
}

static wb_node *
node_new(void *key, void *datum)
{
	wb_node *node;

	if ((node = MALLOC(sizeof(*node))) == NULL)
		return NULL;

	node->key = key;
	node->datum = datum;
	node->parent = NULL;
	node->llink = NULL;
	node->rlink = NULL;
	node->weight = 2;

	return node;
}

static wb_node *
node_min(wb_node *node)
{
	ASSERT(node != NULL);

	while (node->llink)
		node = node->llink;
	return node;
}

static wb_node *
node_max(wb_node *node)
{
	ASSERT(node != NULL);

	while (node->rlink)
		node = node->rlink;
	return node;
}

static wb_node *
node_next(wb_node *node)
{
	wb_node *temp;

	ASSERT(node != NULL);

	if (node->rlink) {
		for (node = node->rlink; node->llink; node = node->llink)
			/* void */;
		return node;
	}
	temp = node->parent;
	while (temp && temp->rlink == node) {
		node = temp;
		temp = temp->parent;
	}
	return temp;
}

static wb_node *
node_prev(wb_node *node)
{
	wb_node *temp;

	ASSERT(node != NULL);

	if (node->llink) {
		for (node = node->llink; node->rlink; node = node->rlink)
			/* void */;
		return node;
	}
	temp = node->parent;
	while (temp && temp->llink == node) {
		node = temp;
		temp = temp->parent;
	}
	return temp;
}

static size_t
node_height(const wb_node *node)
{
	size_t l, r;

	ASSERT(node != NULL);

	l = node->llink ? node_height(node->llink) + 1 : 0;
	r = node->rlink ? node_height(node->rlink) + 1 : 0;
	return MAX(l, r);
}

static size_t
node_mheight(const wb_node *node)
{
	size_t l, r;

	ASSERT(node != NULL);

	l = node->llink ? node_mheight(node->llink) + 1 : 0;
	r = node->rlink ? node_mheight(node->rlink) + 1 : 0;
	return MIN(l, r);
}

static uint64_t
node_pathlen(const wb_node *node, uint64_t level)
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
 * Only the weights of B and RIGHT(B) need to be readjusted, because upper
 * level nodes' weights haven't changed.
 */
static void
rot_left(wb_tree *tree, wb_node *node)
{
	wb_node *rlink, *parent;

	ASSERT(tree != NULL);
	ASSERT(node != NULL);
	ASSERT(node->rlink != NULL);

	rlink = node->rlink;
	node->rlink = rlink->llink;
	if (rlink->llink)
		rlink->llink->parent = node;
	parent = node->parent;
	rlink->parent = parent;
	if (parent) {
		if (parent->llink == node)
			parent->llink = rlink;
		else
			parent->rlink = rlink;
	} else {
		tree->root = rlink;
	}
	rlink->llink = node;
	node->parent = rlink;

	REWEIGH(node);
	REWEIGH(rlink);
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
 * Only the weights of D and LEFT(D) need to be readjusted, because upper level
 * nodes' weights haven't changed.
 */
static void
rot_right(wb_tree *tree, wb_node *node)
{
	wb_node *llink, *parent;

	ASSERT(tree != NULL);
	ASSERT(node != NULL);
	ASSERT(node->llink != NULL);

	llink = node->llink;
	node->llink = llink->rlink;
	if (llink->rlink)
		llink->rlink->parent = node;
	parent = node->parent;
	llink->parent = parent;
	if (parent) {
		if (parent->llink == node)
			parent->llink = llink;
		else
			parent->rlink = llink;
	} else {
		tree->root = llink;
	}
	llink->rlink = node;
	node->parent = llink;

	REWEIGH(node);
	REWEIGH(llink);
}

wb_itor *
wb_itor_new(wb_tree *tree)
{
	wb_itor *itor;

	ASSERT(tree != NULL);

	itor = MALLOC(sizeof(*itor));
	if (itor) {
		itor->tree = tree;
		wb_itor_first(itor);
	}
	return itor;
}

dict_itor *
wb_dict_itor_new(wb_tree *tree)
{
	dict_itor *itor;

	ASSERT(tree != NULL);

	if ((itor = MALLOC(sizeof(*itor))) == NULL)
		return NULL;

	if ((itor->_itor = wb_itor_new(tree)) == NULL) {
		FREE(itor);
		return NULL;
	}

	itor->_vtable = &wb_tree_itor_vtable;

	return itor;
}

void
wb_itor_free(wb_itor *itor)
{
	ASSERT(itor != NULL);

	FREE(itor);
}

#define RETVALID(itor)		return itor->node != NULL

int
wb_itor_valid(const wb_itor *itor)
{
	ASSERT(itor != NULL);

	RETVALID(itor);
}

void
wb_itor_invalidate(wb_itor *itor)
{
	ASSERT(itor != NULL);

	itor->node = NULL;
}

int
wb_itor_next(wb_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		wb_itor_first(itor);
	else
		itor->node = node_next(itor->node);
	RETVALID(itor);
}

int
wb_itor_prev(wb_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		wb_itor_last(itor);
	else
		itor->node = node_prev(itor->node);
	RETVALID(itor);
}

int
wb_itor_nextn(wb_itor *itor, size_t count)
{
	ASSERT(itor != NULL);

	if (count) {
		if (itor->node == NULL) {
			wb_itor_first(itor);
			count--;
		}

		while (count-- && itor->node)
			itor->node = node_next(itor->node);
	}

	RETVALID(itor);
}

int
wb_itor_prevn(wb_itor *itor, size_t count)
{
	ASSERT(itor != NULL);

	if (count) {
		if (itor->node == NULL) {
			wb_itor_last(itor);
			count--;
		}

		while (count-- && itor->node)
			itor->node = node_prev(itor->node);
	}

	RETVALID(itor);
}

int
wb_itor_first(wb_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->tree->root == NULL)
		itor->node = NULL;
	else
		itor->node = node_min(itor->tree->root);
	RETVALID(itor);
}

int
wb_itor_last(itor)
	wb_itor *itor;
{
	ASSERT(itor != NULL);

	if (itor->tree->root == NULL)
		itor->node = NULL;
	else
		itor->node = node_max(itor->tree->root);
	RETVALID(itor);
}

int
wb_itor_search(wb_itor *itor, const void *key)
{
	int cmp;
	wb_node *node;
	dict_compare_func cmp_func;

	ASSERT(itor != NULL);

	cmp_func = itor->tree->cmp_func;
	for (node = itor->tree->root; node;) {
		cmp = cmp_func(key, node->key);
		if (cmp < 0)
			node = node->llink;
		else if (cmp > 0)
			node = node->rlink;
		else {
			itor->node = node;
			return TRUE;
		}
	}
	itor->node = NULL;
	return FALSE;
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

const void *
wb_itor_cdata(const wb_itor *itor)
{
	ASSERT(itor != NULL);

	return itor->node ? itor->node->datum : NULL;
}

int
wb_itor_set_data(wb_itor *itor, void *datum, void **old_datum)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		return -1;

	if (old_datum)
		*old_datum = itor->node->datum;
	itor->node->datum = datum;
	return 0;
}

static void
wb_node_verify(wb_node *node, wb_node *parent)
{
	if (node) {
		ASSERT(node->parent == parent);
		wb_node_verify(node->llink, node);
		wb_node_verify(node->rlink, node);
	}
}

void
wb_tree_verify(const wb_tree *tree)
{
	ASSERT(tree != NULL);

	wb_node_verify(tree->root, NULL);
}
