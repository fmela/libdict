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

#include <stdlib.h>

#include "pr_tree.h"
#include "dict_private.h"

typedef unsigned int weight_t;

typedef struct pr_node pr_node;
struct pr_node {
	void*				key;
	void*				datum;
	pr_node*			parent;
	pr_node*			llink;
	pr_node*			rlink;
	weight_t			weight;
};

#define WEIGHT(n)	((n) ? (n)->weight : 1)
#define REWEIGH(n)	(n)->weight = WEIGHT((n)->llink) + WEIGHT((n)->rlink)

struct pr_tree {
	pr_node*			root;
	size_t				count;
	dict_compare_func	cmp_func;
	dict_delete_func	del_func;
};

struct pr_itor {
	pr_tree*			tree;
	pr_node*			node;
};

static dict_vtable pr_tree_vtable = {
	(dict_inew_func)		pr_dict_itor_new,
	(dict_dfree_func)		pr_tree_free,
	(dict_insert_func)		pr_tree_insert,
	(dict_probe_func)		pr_tree_probe,
	(dict_search_func)		pr_tree_search,
	(dict_remove_func)		pr_tree_remove,
	(dict_clear_func)		pr_tree_clear,
	(dict_traverse_func)	pr_tree_traverse,
	(dict_count_func)		pr_tree_count
};

static itor_vtable pr_tree_itor_vtable = {
	(dict_ifree_func)		pr_itor_free,
	(dict_valid_func)		pr_itor_valid,
	(dict_invalidate_func)	pr_itor_invalidate,
	(dict_next_func)		pr_itor_next,
	(dict_prev_func)		pr_itor_prev,
	(dict_nextn_func)		pr_itor_nextn,
	(dict_prevn_func)		pr_itor_prevn,
	(dict_first_func)		pr_itor_first,
	(dict_last_func)		pr_itor_last,
	(dict_key_func)			pr_itor_key,
	(dict_data_func)		pr_itor_data,
	(dict_dataset_func)		pr_itor_set_data,
	(dict_iremove_func)		NULL,/* pr_itor_remove not implemented yet */
	(dict_icompare_func)	NULL /* pr_itor_compare not implemented yet */
};

static void		fixup(pr_tree *tree, pr_node *node);
static void		rot_left(pr_tree *tree, pr_node *node);
static void		rot_right(pr_tree *tree, pr_node *node);
static size_t	node_height(const pr_node *node);
static size_t	node_mheight(const pr_node *node);
static uint64_t	node_pathlen(const pr_node *node, uint64_t level);
static pr_node*	node_new(void *key, void *datum);
static pr_node*	node_min(pr_node *node);
static pr_node*	node_max(pr_node *node);
static pr_node*	node_next(pr_node *node);
static pr_node*	node_prev(pr_node *node);

pr_tree *
pr_tree_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
	pr_tree *tree;

	if ((tree = MALLOC(sizeof(*tree))) == NULL)
		return NULL;

	tree->root = NULL;
	tree->count = 0;
	tree->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	tree->del_func = del_func;

	return tree;
}

dict *
pr_dict_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
	dict *dct;
	pr_tree *tree;

	if ((dct = MALLOC(sizeof(*dct))) == NULL)
		return NULL;

	if ((tree = pr_tree_new(cmp_func, del_func)) == NULL) {
		FREE(dct);
		return NULL;
	}

	dct->_object = tree;
	dct->_vtable = &pr_tree_vtable;

	return dct;
}

size_t
pr_tree_free(pr_tree *tree)
{
	size_t count = 0;

	ASSERT(tree != NULL);

	if (tree->root)
		count = pr_tree_clear(tree);

	FREE(tree);

	return count;
}

void *
pr_tree_search(pr_tree *tree, const void *key)
{
	int cmp;
	pr_node *node;

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

static void
fixup(pr_tree *tree, pr_node *node)
{
	pr_node *temp;
	weight_t wl, wr;

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
again:
	wl = WEIGHT(node->llink);
	wr = WEIGHT(node->rlink);
	if (wr > wl) {
		ASSERT(node->rlink != NULL);
		if (WEIGHT(node->rlink->rlink) > wl) {			/* LL */
			rot_left(tree, node);
			goto again;
		} else if (WEIGHT(node->rlink->llink) > wl) {	/* RL */
			temp = node->rlink;
			rot_right(tree, temp);
			rot_left(tree, node);
			if (temp->rlink)
				fixup(tree, temp->rlink);
			goto again;
		}
	} else if (wl > wr) {
		ASSERT(node->llink != NULL);
		if (WEIGHT(node->llink->llink) > wr) {			/* RR */
			rot_right(tree, node);
			goto again;
		} else if (WEIGHT(node->llink->rlink) > wr) {	/* LR */
			temp = node->llink;
			rot_left(tree, temp);
			rot_right(tree, node);
			if (temp->llink)
				fixup(tree, temp->llink);
			goto again;
		}
	}
}

/*
 * Macro expansion of above function. Because path reduction is recursive, and
 * we cannot have a macro "call" itself, we use the actual fixup() function.
 */
#define FIXUP(t,node)														\
{																			\
	pr_node *n, *temp;														\
	weight_t wl, wr;														\
																			\
	n = (node);																\
again:																		\
	wl = WEIGHT(n->llink);													\
	wr = WEIGHT(n->rlink);													\
	if (wr > wl) {															\
		ASSERT(node->rlink != NULL);										\
		if (WEIGHT(n->rlink->rlink) > wl) {			/* LL */				\
			rot_left((t), n);												\
			goto again;														\
		} else if (WEIGHT(n->rlink->llink) > wl) {	/* RL */				\
			temp = n->rlink;												\
			rot_right((t), temp);											\
			rot_left((t), n);												\
			if (temp->rlink)												\
				fixup((t), temp->rlink);									\
			goto again;														\
		}																	\
	} else if (wl > wr) {													\
		ASSERT(node->llink != NULL);										\
		if (WEIGHT(n->llink->llink) > wr) {			/* RR */				\
			rot_right((t), n);												\
			goto again;														\
		} else if (WEIGHT(n->llink->rlink) > wr) {	/* LR */				\
			temp = n->llink;												\
			rot_left((t), temp);											\
			rot_right((t), n);												\
			if (temp->llink)												\
				fixup((t), temp->llink);									\
			goto again;														\
		}																	\
	}																		\
}

int
pr_tree_insert(pr_tree *tree, void *key, void *datum, bool overwrite)
{
	int cmp = 0;
	pr_node *node, *parent = NULL;

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
		parent = parent->parent;
		node->weight++;
		FIXUP(tree, node);
	}

	tree->count++;
	return 0;
}

int
pr_tree_probe(pr_tree *tree, void *key, void **datum)
{
	int cmp = 0;
	pr_node *node, *parent = NULL;

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
		return 1;
	}

	if (cmp < 0)
		parent->llink = node;
	else
		parent->rlink = node;

	while ((node = parent) != NULL) {
		parent = parent->parent;
		node->weight++;
		FIXUP(tree, node);
	}

	tree->count++;
	return 1;
}

int
pr_tree_remove(pr_tree *tree, const void *key)
{
	int cmp;
	pr_node *node, *temp, *out = NULL; /* ergh @ GCC unitializated warning */

	ASSERT(tree != NULL);
	ASSERT(key != NULL);

	node = tree->root;
	while (node) {
		cmp = tree->cmp_func(key, node->key);
		if (cmp) {
			node = cmp < 0 ? node->llink : node->rlink;
			continue;
		}
		if (node->llink == NULL) {
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
			return 0;
		} else if (node->rlink == NULL) {
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
			return 0;
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
	return -1;
}

size_t
pr_tree_clear(pr_tree *tree)
{
	pr_node *node, *parent;
	size_t count;

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
pr_tree_min(const pr_tree *tree)
{
	const pr_node *node;

	ASSERT(tree != NULL);

	if ((node = tree->root) == NULL)
		return NULL;

	for (; node->llink; node = node->llink)
		/* void */;
	return node->key;
}

const void *
pr_tree_max(const pr_tree *tree)
{
	const pr_node *node;

	ASSERT(tree != NULL);

	if ((node = tree->root) == NULL)
		return NULL;

	for (; node->rlink; node = node->rlink)
		/* void */;
	return node->key;
}

size_t
pr_tree_traverse(pr_tree *tree, dict_visit_func visit)
{
	pr_node *node;
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
pr_tree_count(const pr_tree *tree)
{
	ASSERT(tree != NULL);

	return tree->count;
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

uint64_t
pr_tree_pathlen(const pr_tree *tree)
{
	ASSERT(tree != NULL);

	return tree->root ? node_pathlen(tree->root, 1) : 0;
}

static pr_node *
node_new(void *key, void *datum)
{
	pr_node *node;

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

static pr_node *
node_min(pr_node *node)
{
	ASSERT(node != NULL);

	while (node->llink)
		node = node->llink;
	return node;
}

static pr_node *
node_max(pr_node *node)
{
	ASSERT(node != NULL);

	while (node->rlink)
		node = node->rlink;
	return node;
}

static pr_node *
node_next(pr_node *node)
{
	pr_node *temp;

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

static pr_node *
node_prev(pr_node *node)
{
	pr_node *temp;

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
node_height(const pr_node *node)
{
	size_t l, r;

	ASSERT(node != NULL);

	l = node->llink ? node_height(node->llink) + 1 : 0;
	r = node->rlink ? node_height(node->rlink) + 1 : 0;
	return MAX(l, r);
}

static size_t
node_mheight(const pr_node *node)
{
	size_t l, r;

	ASSERT(node != NULL);

	l = node->llink ? node_mheight(node->llink) + 1 : 0;
	r = node->rlink ? node_mheight(node->rlink) + 1 : 0;
	return MIN(l, r);
}

static uint64_t
node_pathlen(const pr_node *node, uint64_t level)
{
	unsigned n = 0;

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
 * Only the weights of B and B->rlink need to be readjusted, because upper
 * level nodes' weights haven't changed.
 */
static void
rot_left(pr_tree *tree, pr_node	*node)
{
	pr_node *rlink, *parent;

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
 * Only the weights of D and D->llink need to be readjusted, because upper level
 * nodes' weights haven't changed.
 */
static void
rot_right(pr_tree *tree, pr_node *node)
{
	pr_node *llink, *parent;

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

pr_itor *
pr_itor_new(pr_tree *tree)
{
	pr_itor *itor;

	ASSERT(tree != NULL);

	if ((itor = MALLOC(sizeof(*itor))) == NULL)
		return NULL;

	itor->tree = tree;
	pr_itor_first(itor);
	return itor;
}

dict_itor *
pr_dict_itor_new(pr_tree *tree)
{
	dict_itor *itor;

	ASSERT(tree != NULL);

	if ((itor = MALLOC(sizeof(*itor))) == NULL)
		return NULL;

	if ((itor->_itor = pr_itor_new(tree)) == NULL) {
		FREE(itor);
		return NULL;
	}

	itor->_vtable = &pr_tree_itor_vtable;

	return itor;
}

void
pr_itor_free(pr_itor *itor)
{
	ASSERT(itor != NULL);

	FREE(itor);
}

#define RETVALID(itor)		return itor->node != NULL

int
pr_itor_valid(const pr_itor *itor)
{
	ASSERT(itor != NULL);

	RETVALID(itor);
}

void
pr_itor_invalidate(pr_itor *itor)
{
	ASSERT(itor != NULL);

	itor->node = NULL;
}

int
pr_itor_next(pr_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		pr_itor_first(itor);
	else
		itor->node = node_next(itor->node);
	RETVALID(itor);
}

int
pr_itor_prev(pr_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		pr_itor_last(itor);
	else
		itor->node = node_prev(itor->node);
	RETVALID(itor);
}

int
pr_itor_nextn(pr_itor *itor, size_t count)
{
	ASSERT(itor != NULL);

	if (count) {
		if (itor->node == NULL) {
			pr_itor_first(itor);
			count--;
		}

		while (count-- && itor->node)
			itor->node = node_next(itor->node);
	}

	RETVALID(itor);
}

int
pr_itor_prevn(pr_itor *itor, size_t count)
{
	ASSERT(itor != NULL);

	if (count) {
		if (itor->node == NULL) {
			pr_itor_last(itor);
			count--;
		}

		while (count-- && itor->node)
			itor->node = node_prev(itor->node);
	}

	RETVALID(itor);
}

int
pr_itor_first(pr_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->tree->root == NULL)
		itor->node = NULL;
	else
		itor->node = node_min(itor->tree->root);
	RETVALID(itor);
}

int
pr_itor_last(pr_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->tree->root == NULL)
		itor->node = NULL;
	else
		itor->node = node_max(itor->tree->root);
	RETVALID(itor);
}

int
pr_itor_search(pr_itor *itor, const void *key)
{
	int cmp;
	pr_node *node;
	dict_compare_func cmp_func;

	ASSERT(itor != NULL);

	cmp_func = itor->tree->cmp_func;
	for (node = itor->tree->root; node;) {
		cmp = cmp_func(key, node->key);
		if (cmp < 0)
			node = node->llink;
		else if (cmp > 0)
			node = node->rlink;
		else
			break;
	}
	itor->node = node;
	RETVALID(itor);
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

int
pr_itor_set_data(pr_itor *itor, void *datum, void **old_datum)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		return -1;

	if (old_datum)
		*old_datum = itor->node->datum;
	itor->node->datum = datum;
	return 0;
}
