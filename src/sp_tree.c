/*
 * libdict -- splay tree implementation.
 * cf. [Sleator and Tarjan, 1985], [Tarjan 1985], [Tarjan 1983]
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
 *
 *
 * A single operation on a splay tree has a worst-case time complexity of O(N),
 * but a series of M operations have a time complexity of O(M lg N), and thus
 * the amortized time complexity of an operation is O(lg N). More specifically,
 * a series of M operations on a tree with N nodes will runs in O((N+M)lg(N+M))
 * time. Splay trees work by "splaying" a node up the tree using a series of
 * rotations until it is the root each time it is accessed. They are much
 * simpler to code than most balanced trees, because there is no strict
 * requirement about maintaining a balance scheme among nodes. When inserting
 * and searching, we simply splay the node in question up until it becomes the
 * root of the tree.
 *
 * This implementation is a bottom-up, move-to-root splay tree.
 */

/* TODO: rather than splay after the fact, use the splay operation to traverse
 * the tree during insert, search, delete, etc. */

#include <stdlib.h>

#include "sp_tree.h"
#include "dict_private.h"

typedef struct sp_node sp_node;
struct sp_node {
	void*				key;
	void*				datum;
	sp_node*			parent;
	sp_node*			llink;
	sp_node*			rlink;
};

struct sp_tree {
	sp_node*			root;
	size_t				count;
	dict_compare_func	cmp_func;
	dict_delete_func	del_func;
};

struct sp_itor {
	sp_tree*			tree;
	sp_node*			node;
};

static dict_vtable sp_tree_vtable = {
	(dict_inew_func)		sp_dict_itor_new,
	(dict_dfree_func)		sp_tree_free,
	(dict_insert_func)		sp_tree_insert,
	(dict_probe_func)		sp_tree_probe,
	(dict_search_func)		sp_tree_search,
	(dict_csearch_func)		sp_tree_search,
	(dict_remove_func)		sp_tree_remove,
	(dict_clear_func)		sp_tree_clear,
	(dict_traverse_func)	sp_tree_traverse,
	(dict_count_func)		sp_tree_count
};

static itor_vtable sp_tree_itor_vtable = {
	(dict_ifree_func)		sp_itor_free,
	(dict_valid_func)		sp_itor_valid,
	(dict_invalidate_func)	sp_itor_invalidate,
	(dict_next_func)		sp_itor_next,
	(dict_prev_func)		sp_itor_prev,
	(dict_nextn_func)		sp_itor_nextn,
	(dict_prevn_func)		sp_itor_prevn,
	(dict_first_func)		sp_itor_first,
	(dict_last_func)		sp_itor_last,
	(dict_key_func)			sp_itor_key,
	(dict_data_func)		sp_itor_data,
	(dict_cdata_func)		sp_itor_cdata,
	(dict_dataset_func)		sp_itor_set_data,
	(dict_iremove_func)		NULL,/* sp_itor_remove not implemented yet */
	(dict_icompare_func)	NULL /* sp_itor_compare not implemented yet */
};

static void		rot_left(sp_tree *tree, sp_node *node);
static void		rot_right(sp_tree *tree, sp_node *node);
static sp_node*	node_new(void *key, void *datum);
static sp_node*	node_next(sp_node *node);
static sp_node*	node_prev(sp_node *node);
static sp_node*	node_max(sp_node *node);
static sp_node*	node_min(sp_node *node);
static size_t	node_height(const sp_node *node);
static size_t	node_mheight(const sp_node *node);
static uint64_t	node_pathlen(const sp_node *node, uint64_t level);

sp_tree *
sp_tree_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
	sp_tree *tree;

	if ((tree = MALLOC(sizeof(*tree))) == NULL)
		return NULL;

	tree->root = NULL;
	tree->count = 0;
	tree->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	tree->del_func = del_func;

	return tree;
}

dict *
sp_dict_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
	dict *dct;

	if ((dct = MALLOC(sizeof(*dct))) == NULL)
		return NULL;

	if ((dct->_object = sp_tree_new(cmp_func, del_func)) == NULL) {
		FREE(dct);
		return NULL;
	}

	dct->_vtable = &sp_tree_vtable;

	return dct;
}

size_t
sp_tree_free(sp_tree *tree)
{
	size_t count = 0;

	ASSERT(tree != NULL);

	if (tree->root)
		count = sp_tree_clear(tree);

	FREE(tree);
	return count;
}

size_t
sp_tree_clear(sp_tree *tree)
{
	sp_node *node, *parent;
	size_t count = 0;

	ASSERT(tree != NULL);

	count = tree->count;
	node = tree->root;
	while (node) {
		parent = node->parent;
		if (node->llink) {
			node = node->llink;
			continue;
		}
		if (node->rlink) {
			node = node->rlink;
			continue;
		}

		if (tree->del_func)
			tree->del_func(node->key, node->datum);
		FREE(node);
		tree->count--;

		if (parent) {
			if (parent->llink == node)
				parent->llink = NULL;
			else
				parent->rlink = NULL;
		}
		node = parent;
	}

	tree->root = NULL;
	ASSERT(tree->count == 0);
	return count;
}

/*
 * XXX Each zig/zig and zig/zag operation can be optimized, but for now we just
 * use two rotations.
 */
/*
 * zig_zig_right(T, A):
 *
 *     C               A
 *    /        B        \
 *   B   ==>  / \  ==>   B
 *  /        A   C        \
 * A                       C
 *
 * zig_zig_left(T, C):
 *
 * A                        C
 *  \           B          /
 *   B    ==>  / \  ==>   B
 *    \       A   C      /
 *     C                A
 *
 * zig_zag_right(T, B)
 *
 * A        A
 *  \        \          B
 *   C  ==>   B   ==>  / \
 *  /          \      A   C
 * B            C
 *
 * zig_zag_left(T, B)
 *
 *   C          C
 *  /          /        B
 * A    ==>   B   ==>  / \
 *  \        /        A   C
 *   B      A
 */
#define SPLAY(t,n)															\
{																			\
	sp_node *p;																\
																			\
	p = (n)->parent;														\
	if (p == (t)->root) {													\
		if (p->llink == (n))				/* zig right */					\
			rot_right((t), p);												\
		else								/* zig left */					\
			rot_left((t), p);												\
	} else {																\
		if (p->llink == (n)) {												\
			if (p->parent->llink == p) {	/* zig zig right */				\
				rot_right((t), p->parent);									\
				rot_right((t), (n)->parent);								\
			} else {						/* zig zag right */				\
				rot_right((t), p);											\
				rot_left((t), (n)->parent);									\
			}																\
		} else {															\
			if (p->parent->rlink == p) {	/* zig zig left */				\
				rot_left((t), p->parent);									\
				rot_left((t), (n)->parent);									\
			} else {						/* zig zag left */				\
				rot_left((t), p);											\
				rot_right((t), (n)->parent);								\
			}																\
		}																	\
	}																		\
}

int
sp_tree_insert(sp_tree *tree, void *key, void *datum, int overwrite)
{
	int cmp = 0; /* shut up GCC */
	sp_node *node, *parent = NULL;

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
	tree->count++;

	while (node->parent)
		SPLAY(tree, node);

	return 0;
}

int
sp_tree_probe(sp_tree *tree, void *key, void **datum)
{
	int cmp = 0;
	sp_node *node, *parent = NULL;

	ASSERT(tree != NULL);

	node = tree->root;
	while (node) {
		cmp = tree->cmp_func(key, node->key);
		if (cmp < 0)
			parent = node, node = node->llink;
		else if (cmp > 0)
			parent = node, node = node->rlink;
		else {
			while (node->parent)
				SPLAY(tree, node);
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
	tree->count++;

	while (node->parent)
		SPLAY(tree, node);

	return 1;
}

void *
sp_tree_search(sp_tree *tree, const void *key)
{
	int cmp;
	sp_node *node, *parent = NULL;

	ASSERT(tree != NULL);

	node = tree->root;
	while (node) {
		cmp = tree->cmp_func(key, node->key);
		if (cmp < 0)
			parent = node, node = node->llink;
		else if (cmp > 0)
			parent = node, node = node->rlink;
		else {
			while (node->parent)
				SPLAY(tree, node);
			return node->datum;
		}
	}
	/* XXX This is questionable. Just because a node is the nearest match
	 * doesn't mean it should become the new root. */
	if (parent)
		while (parent->parent)
			SPLAY(tree, parent);
	return NULL;
}

int
sp_tree_remove(sp_tree *tree, const void *key)
{
	int cmp;
	sp_node *node, *temp, *out, *parent;

	ASSERT(tree != NULL);

	node = tree->root;
	while (node) {
		cmp = tree->cmp_func(key, node->key);
		if (cmp < 0)
			node = node->llink;
		else if (cmp > 0)
			node = node->rlink;
		else
			break;
	}

	if (node == NULL)
		return -1;

	if (node->llink == NULL || node->rlink == NULL) {
		out = node;
	} else {
		void *tmp;
		/*
		 * This is sure to screw up iterators that were positioned at the node
		 * "out".
		 */
		for (out = node->rlink; out->llink; out = out->llink)
			/* void */;
		SWAP(node->key, out->key, tmp);
		SWAP(node->datum, out->datum, tmp);
	}

	temp = out->llink ? out->llink : out->rlink;
	parent = out->parent;
	if (temp)
		temp->parent = parent;
	if (parent) {
		if (parent->llink == out)
			parent->llink = temp;
		else
			parent->rlink = temp;
	} else {
		tree->root = temp;
	}
	if (tree->del_func)
		tree->del_func(out->key, out->datum);

	/* Splay an adjacent node to the root, if possible. */
	temp =
		node->parent ? node->parent :
		node->rlink ? node->rlink :
		node->llink;
	if (temp)
		while (temp->parent)
			SPLAY(tree, temp);

	FREE(out);
	tree->count--;

	return 0;
}

size_t
sp_tree_traverse(sp_tree *tree, dict_visit_func visit)
{
	sp_node *node;
	size_t count = 0;

	ASSERT(tree != NULL);
	ASSERT(visit != NULL);

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
sp_tree_count(const sp_tree *tree)
{
	ASSERT(tree != NULL);

	return tree->count;
}

size_t
sp_tree_height(const sp_tree *tree)
{
	ASSERT(tree != NULL);

	return tree->root ? node_height(tree->root) : 0;
}

size_t
sp_tree_mheight(const sp_tree *tree)
{
	ASSERT(tree != NULL);

	return tree->root ? node_mheight(tree->root) : 0;
}

uint64_t
sp_tree_pathlen(const sp_tree *tree)
{
	ASSERT(tree != NULL);

	return tree->root ? node_pathlen(tree->root, 1) : 0;
}

const void *
sp_tree_min(const sp_tree *tree)
{
	const sp_node *node;

	ASSERT(tree != NULL);

	if ((node = tree->root) == NULL)
		return NULL;

	for (; node->llink; node = node->llink)
		/* void */;
	return node->key;
}

const void *
sp_tree_max(const sp_tree *tree)
{
	const sp_node *node;

	ASSERT(tree != NULL);

	if ((node = tree->root) == NULL)
		return NULL;

	for (; node->rlink; node = node->rlink)
		/* void */;
	return node->key;
}

static void
rot_left(sp_tree *tree, sp_node *node)
{
	sp_node *rlink, *parent;

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
}

static void
rot_right(sp_tree *tree, sp_node *node)
{
	sp_node *llink, *parent;

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
}

static sp_node *
node_new(void *key, void *datum)
{
	sp_node *node;

	if ((node = MALLOC(sizeof(*node))) == NULL)
		return NULL;

	node->key = key;
	node->datum = datum;
	node->parent = NULL;
	node->llink = NULL;
	node->rlink = NULL;

	return node;
}

static sp_node *
node_next(sp_node *node)
{
	sp_node *temp;

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

static sp_node *
node_prev(sp_node *node)
{
	sp_node *temp;

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

static sp_node *
node_max(sp_node *node)
{
	ASSERT(node != NULL);

	while (node->rlink)
		node = node->rlink;
	return node;
}

static sp_node *
node_min(sp_node *node)
{
	ASSERT(node != NULL);

	while (node->llink)
		node = node->llink;
	return node;
}

static size_t
node_height(const sp_node *node)
{
	size_t l, r;

	l = node->llink ? node_height(node->llink) + 1 : 0;
	r = node->rlink ? node_height(node->rlink) + 1 : 0;
	return MAX(l, r);
}

static size_t
node_mheight(const sp_node *node)
{
	size_t l, r;

	l = node->llink ? node_mheight(node->llink) + 1 : 0;
	r = node->rlink ? node_mheight(node->rlink) + 1 : 0;
	return MIN(l, r);
}

static uint64_t
node_pathlen(const sp_node *node, uint64_t level)
{
	size_t n = 0;

	ASSERT(node != NULL);

	if (node->llink)
		n += level + node_pathlen(node->llink, level + 1);
	if (node->rlink)
		n += level + node_pathlen(node->rlink, level + 1);
	return n;
}

sp_itor *
sp_itor_new(sp_tree *tree)
{
	sp_itor *itor;

	ASSERT(tree != NULL);

	if ((itor = MALLOC(sizeof(*itor))) == NULL)
		return NULL;

	itor->tree = tree;
	sp_itor_first(itor);
	return itor;
}

dict_itor *
sp_dict_itor_new(sp_tree *tree)
{
	dict_itor *itor;

	ASSERT(tree != NULL);

	if ((itor = MALLOC(sizeof(*itor))) == NULL)
		return NULL;

	if ((itor->_itor = sp_itor_new(tree)) == NULL) {
		FREE(itor);
		return NULL;
	}

	itor->_vtable = &sp_tree_itor_vtable;

	return itor;
}

void
sp_itor_free(sp_itor *itor)
{
	ASSERT(itor != NULL);

	FREE(itor);
}

#define RETVALID(itor)		return itor->node != NULL

int
sp_itor_valid(const sp_itor *itor)
{
	ASSERT(itor != NULL);

	RETVALID(itor);
}

void
sp_itor_invalidate(sp_itor *itor)
{
	ASSERT(itor != NULL);

	itor->node = NULL;
}

int
sp_itor_next(sp_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		sp_itor_first(itor);
	else
		itor->node = node_next(itor->node);
	RETVALID(itor);
}

int
sp_itor_prev(sp_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		sp_itor_last(itor);
	else
		itor->node = node_prev(itor->node);
	RETVALID(itor);
}

int
sp_itor_nextn(sp_itor *itor, size_t count)
{
	ASSERT(itor != NULL);

	if (count) {
		if (itor->node == NULL) {
			sp_itor_first(itor);
			count--;
		}

		while (count-- && itor->node)
			itor->node = node_next(itor->node);
	}

	RETVALID(itor);
}

int
sp_itor_prevn(sp_itor *itor, size_t count)
{
	ASSERT(itor != NULL);

	if (count) {
		if (itor->node == NULL) {
			sp_itor_last(itor);
			count--;
		}

		while (count-- && itor->node)
			itor->node = node_prev(itor->node);
	}

	RETVALID(itor);
}

int
sp_itor_first(sp_itor *itor)
{
	sp_node *r;

	ASSERT(itor != NULL);

	r = itor->tree->root;
	itor->node = r ? node_min(r) : NULL;
	RETVALID(itor);
}

int
sp_itor_last(sp_itor *itor)
{
	sp_node *r;

	ASSERT(itor != NULL);

	r = itor->tree->root;
	itor->node = r ? node_max(r) : NULL;
	RETVALID(itor);
}

int
sp_itor_search(sp_itor *itor, const void *key)
{
	int cmp;
	sp_node *node;
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
sp_itor_key(const sp_itor *itor)
{
	ASSERT(itor != NULL);

	return itor->node ? itor->node->key : NULL;
}

void *
sp_itor_data(sp_itor *itor)
{
	ASSERT(itor != NULL);

	return itor->node ? itor->node->datum : NULL;
}

const void *
sp_itor_cdata(const sp_itor *itor)
{
	ASSERT(itor != NULL);

	return itor->node ? itor->node->datum : NULL;
}

int
sp_itor_set_data(sp_itor *itor, void *datum, void **old_datum)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		return -1;

	if (old_datum)
		*old_datum = itor->node->datum;
	itor->node->datum = datum;
	return 0;
}
