/*
 * rb_tree.c
 *
 * Implementation of red-black binary search tree.
 * Copyright (C) 2001-2010 Farooq Mela.
 *
 * $Id$
 *
 * cf. [Cormen, Leiserson, and Rivest 1990], [Guibas and Sedgewick, 1978]
 */

#include <stdlib.h>

#include "rb_tree.h"
#include "dict_private.h"

typedef struct rb_node rb_node;
struct rb_node {
	void*				key;
	void*				datum;
	rb_node*			parent;
	rb_node*			llink;
	rb_node*			rlink;
	unsigned			color:1;
};

#define RB_RED			0
#define RB_BLK			1

struct rb_tree {
	rb_node*			root;
	unsigned			count;
	dict_compare_func	key_cmp;
	dict_delete_func	del_func;
};

struct rb_itor {
	rb_tree*			tree;
	rb_node*			node;
};

static dict_vtable rb_tree_vtable = {
	(inew_func)			rb_dict_itor_new,
	(destroy_func)		rb_tree_destroy,
	(insert_func)		rb_tree_insert,
	(probe_func)		rb_tree_probe,
	(search_func)		rb_tree_search,
	(csearch_func)		rb_tree_csearch,
	(remove_func)		rb_tree_remove,
	(empty_func)		rb_tree_empty,
	(walk_func)			rb_tree_walk,
	(count_func)		rb_tree_count
};

static itor_vtable rb_tree_itor_vtable = {
	(idestroy_func)		rb_itor_destroy,
	(valid_func)		rb_itor_valid,
	(invalidate_func)	rb_itor_invalidate,
	(next_func)			rb_itor_next,
	(prev_func)			rb_itor_prev,
	(nextn_func)		rb_itor_nextn,
	(prevn_func)		rb_itor_prevn,
	(first_func)		rb_itor_first,
	(last_func)			rb_itor_last,
	(key_func)			rb_itor_key,
	(data_func)			rb_itor_data,
	(cdata_func)		rb_itor_cdata,
	(dataset_func)		rb_itor_set_data,
	(iremove_func)		NULL,/* rb_itor_remove not implemented yet */
	(compare_func)		NULL /* rb_itor_compare not implemented yet */
};

static rb_node _null = { NULL, NULL, NULL, NULL, NULL, RB_BLK };
#define RB_NULL	&_null

static void		rot_left(rb_tree *tree, rb_node *node);
static void		rot_right(rb_tree *tree, rb_node *node);
static void		insert_fixup(rb_tree *tree, rb_node *node);
static void		delete_fixup(rb_tree *tree, rb_node *node);
static unsigned	node_height(const rb_node *node);
static unsigned	node_mheight(const rb_node *node);
static unsigned	node_pathlen(const rb_node *node, unsigned level);
static rb_node*	node_new(void *key, void *datum);
static rb_node*	node_next(rb_node *node);
static rb_node*	node_prev(rb_node *node);
static rb_node*	node_max(rb_node *node);
static rb_node*	node_min(rb_node *node);

rb_tree *
rb_tree_new(dict_compare_func key_cmp, dict_delete_func del_func)
{
	rb_tree *tree;

	if ((tree = MALLOC(sizeof(*tree))) == NULL)
		return NULL;

	tree->root = RB_NULL;
	tree->count = 0;
	tree->key_cmp = key_cmp ? key_cmp : dict_ptr_cmp;
	tree->del_func = del_func;

	return tree;
}

dict *
rb_dict_new(dict_compare_func key_cmp, dict_delete_func del_func)
{
	dict *dct;
	rb_tree *tree;

	if ((dct = MALLOC(sizeof(*dct))) == NULL)
		return NULL;

	if ((tree = rb_tree_new(key_cmp, del_func)) == NULL) {
		FREE(dct);
		return NULL;
	}

	dct->_object = tree;
	dct->_vtable = &rb_tree_vtable;

	return dct;
}

unsigned
rb_tree_destroy(rb_tree *tree)
{
	unsigned count = 0;

	ASSERT(tree != NULL);

	if (tree->root != RB_NULL)
		count = rb_tree_empty(tree);

	FREE(tree);

	return count;
}

void *
rb_tree_search(rb_tree *tree, const void *key)
{
	rb_node *node;

	ASSERT(tree != NULL);

	node = tree->root;
	while (node != RB_NULL) {
		int cmp = tree->key_cmp(key, node->key);
		if (cmp < 0)
			node = node->llink;
		else if (cmp > 0)
			node = node->rlink;
		else
			return node->datum;
	}

	return NULL;
}

const void *
rb_tree_csearch(const rb_tree *tree, const void *key)
{
	ASSERT(tree != NULL);

	return rb_tree_search((rb_tree *)tree, key);
}

int
rb_tree_insert(rb_tree *tree, void *key, void *datum, int overwrite)
{
	int cmp = 0;	/* Quell GCC warning about uninitialized usage. */
	rb_node *node, *parent = RB_NULL;

	ASSERT(tree != NULL);

	node = tree->root;

	while (node != RB_NULL) {
		cmp = tree->key_cmp(key, node->key);
		if (cmp < 0)
			parent = node, node = node->llink;
		else if (cmp > 0)
			parent = node, node = node->rlink;
		else {
			if (overwrite == 0)
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

	if ((node->parent = parent) == RB_NULL) {
		tree->root = node;
		ASSERT(tree->count == 0);
		tree->count = 1;
		node->color = RB_BLK;
		return 0;
	}
	if (cmp < 0)
		parent->llink = node;
	else
		parent->rlink = node;

	insert_fixup(tree, node);
	tree->count++;
	return 0;
}

int
rb_tree_probe(rb_tree *tree, void *key, void **datum)
{
	int cmp = 0;
	rb_node *node, *parent = RB_NULL;

	ASSERT(tree != NULL);

	node = tree->root;

	while (node != RB_NULL) {
		cmp = tree->key_cmp(key, node->key);
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

	if ((node->parent = parent) == RB_NULL) {
		tree->root = node;
		ASSERT(tree->count == 0);
		tree->count = 1;
		node->color = RB_BLK;
		return 1;
	}
	if (cmp < 0)
		parent->llink = node;
	else
		parent->rlink = node;

	insert_fixup(tree, node);
	tree->count++;
	return 1;
}

static void
insert_fixup(rb_tree *tree, rb_node	*node)
{
	rb_node *temp;

	ASSERT(tree != NULL);
	ASSERT(node != NULL);

	while (node != tree->root && node->parent->color == RB_RED) {
		if (node->parent == node->parent->parent->llink) {
			temp = node->parent->parent->rlink;
			if (temp->color == RB_RED) {
				temp->color = RB_BLK;
				node = node->parent;
				node->color = RB_BLK;
				node = node->parent;
				node->color = RB_RED;
			} else {
				if (node == node->parent->rlink) {
					node = node->parent;
					rot_left(tree, node);
				}
				temp = node->parent;
				temp->color = RB_BLK;
				temp = temp->parent;
				temp->color = RB_RED;
				rot_right(tree, temp);
			}
		} else {
			temp = node->parent->parent->llink;
			if (temp->color == RB_RED) {
				temp->color = RB_BLK;
				node = node->parent;
				node->color = RB_BLK;
				node = node->parent;
				node->color = RB_RED;
			} else {
				if (node == node->parent->llink) {
					node = node->parent;
					rot_right(tree, node);
				}
				temp = node->parent;
				temp->color = RB_BLK;
				temp = temp->parent;
				temp->color = RB_RED;
				rot_left(tree, temp);
			}
		}
	}

	tree->root->color = RB_BLK;
}

int
rb_tree_remove(rb_tree *tree, const void *key)
{
	rb_node *node, *temp, *out, *parent;

	ASSERT(tree != NULL);

	node = tree->root;
	while (node != RB_NULL) {
		int cmp = tree->key_cmp(key, node->key);
		if (cmp < 0)
			node = node->llink;
		else if (cmp > 0)
			node = node->rlink;
		else
			break;
	}
	if (node == RB_NULL)
		return -1;

	if (node->llink == RB_NULL || node->rlink == RB_NULL) {
		out = node;
	} else {
		void *tmp;
		for (out = node->rlink; out->llink != RB_NULL; out = out->llink)
			/* void */;
		SWAP(node->key, out->key, tmp);
		SWAP(node->datum, out->datum, tmp);
	}

	temp = out->llink != RB_NULL ? out->llink : out->rlink;
	parent = out->parent;
	temp->parent = parent;
	if (parent != RB_NULL) {
		if (parent->llink == out)
			parent->llink = temp;
		else
			parent->rlink = temp;
	} else {
		tree->root = temp;
	}

	if (out->color == RB_BLK)
		delete_fixup(tree, temp);
	if (tree->del_func)
		tree->del_func(out->key, out->datum);
	FREE(out);

	tree->count--;

	return 0;
}

static void
delete_fixup(rb_tree *tree, rb_node *node)
{
	rb_node *temp;

	ASSERT(tree != NULL);
	ASSERT(node != NULL);

	while (node != tree->root && node->color == RB_BLK) {
		if (node->parent->llink == node) {
			temp = node->parent->rlink;
			if (temp->color == RB_RED) {
				temp->color = RB_BLK;
				node->parent->color = RB_RED;
				rot_left(tree, node->parent);
				temp = node->parent->rlink;
			}
			if (temp->llink->color == RB_BLK && temp->rlink->color == RB_BLK) {
				temp->color = RB_RED;
				node = node->parent;
			} else {
				if (temp->rlink->color == RB_BLK) {
					temp->llink->color = RB_BLK;
					temp->color = RB_RED;
					rot_right(tree, temp);
					temp = node->parent->rlink;
				}
				temp->color = node->parent->color;
				temp->rlink->color = RB_BLK;
				node->parent->color = RB_BLK;
				rot_left(tree, node->parent);
				break;
			}
		} else {
			temp = node->parent->llink;
			if (temp->color == RB_RED) {
				temp->color = RB_BLK;
				node->parent->color = RB_RED;
				rot_right(tree, node->parent);
				temp = node->parent->llink;
			}
			if (temp->rlink->color == RB_BLK && temp->llink->color == RB_BLK) {
				temp->color = RB_RED;
				node = node->parent;
			} else {
				if (temp->llink->color == RB_BLK) {
					temp->rlink->color = RB_BLK;
					temp->color = RB_RED;
					rot_left(tree, temp);
					temp = node->parent->llink;
				}
				temp->color = node->parent->color;
				node->parent->color = RB_BLK;
				temp->llink->color = RB_BLK;
				rot_right(tree, node->parent);
				break;
			}
		}
	}

	node->color = RB_BLK;
}

unsigned
rb_tree_empty(rb_tree *tree)
{
	rb_node *node, *parent;
	unsigned count;

	ASSERT(tree != NULL);

	count = tree->count;
	node = tree->root;
	while (node != RB_NULL) {
		parent = node->parent;
		if (node->llink != RB_NULL) {
			node = node->llink;
			continue;
		}
		if (node->rlink != RB_NULL) {
			node = node->rlink;
			continue;
		}

		if (tree->del_func)
			tree->del_func(node->key, node->datum);
		FREE(node);
		tree->count--;

		if (parent != RB_NULL) {
			if (parent->llink == node)
				parent->llink = RB_NULL;
			else
				parent->rlink = RB_NULL;
		}
		node = parent;
	}

	tree->root = RB_NULL;
	ASSERT(tree->count == 0);
	return count;
}

unsigned
rb_tree_count(const rb_tree *tree)
{
	ASSERT(tree != NULL);

	return tree->count;
}

unsigned
rb_tree_height(const rb_tree *tree)
{
	ASSERT(tree != NULL);

	return tree->root != RB_NULL ? node_height(tree->root) : 0;
}

unsigned
rb_tree_mheight(const rb_tree *tree)
{
	ASSERT(tree != NULL);

	return tree->root != RB_NULL ? node_mheight(tree->root) : 0;
}

unsigned
rb_tree_pathlen(const rb_tree *tree)
{
	ASSERT(tree != NULL);

	return tree->root != RB_NULL ? node_pathlen(tree->root, 1) : 0;
}

const void *
rb_tree_min(const rb_tree *tree)
{
	const rb_node *node;

	ASSERT(tree != NULL);

	if ((node = tree->root) == RB_NULL)
		return NULL;

	for (; node->llink != RB_NULL; node = node->llink)
		/* void */;
	return node->key;
}

const void *
rb_tree_max(const rb_tree *tree)
{
	const rb_node *node;

	ASSERT(tree != NULL);

	if ((node = tree->root) == RB_NULL)
		return NULL;

	for (; node->rlink != RB_NULL; node = node->rlink)
		/* void */;
	return node->key;
}

unsigned
rb_tree_walk(rb_tree *tree, dict_visit_func visit)
{
	rb_node *node;
	unsigned count = 0;

	ASSERT(tree != NULL);
	ASSERT(visit != NULL);

	if (tree->root == RB_NULL)
		return 0;

	for (node = node_min(tree->root); node != RB_NULL; node = node_next(node)) {
		++count;
		if (!visit(node->key, node->datum))
			break;
	}
	return count;
}

static unsigned
node_height(const rb_node *node)
{
	unsigned l, r;

	l = node->llink != RB_NULL ? node_height(node->llink) + 1 : 0;
	r = node->rlink != RB_NULL ? node_height(node->rlink) + 1 : 0;
	return MAX(l, r);
}

static unsigned
node_mheight(node)
	const rb_node *node;
{
	unsigned l, r;

	l = node->llink != RB_NULL ? node_mheight(node->llink) + 1 : 0;
	r = node->rlink != RB_NULL ? node_mheight(node->rlink) + 1 : 0;
	return MIN(l, r);
}

static unsigned
node_pathlen(const rb_node *node, unsigned level)
{
	unsigned n = 0;

	ASSERT(node != RB_NULL);

	if (node->llink != RB_NULL)
		n += level + node_pathlen(node->llink, level + 1);
	if (node->rlink != RB_NULL)
		n += level + node_pathlen(node->rlink, level + 1);
	return n;
}

static void
rot_left(rb_tree *tree, rb_node *node)
{
	rb_node *rlink, *parent;

	ASSERT(tree != NULL);
	ASSERT(node != NULL);

	rlink = node->rlink;
	node->rlink = rlink->llink;
	if (rlink->llink != RB_NULL)
		rlink->llink->parent = node;
	parent = node->parent;
	rlink->parent = parent;
	if (parent != RB_NULL) {
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
rot_right(rb_tree *tree, rb_node *node)
{
	rb_node *llink, *parent;

	ASSERT(tree != NULL);
	ASSERT(node != NULL);

	llink = node->llink;
	node->llink = llink->rlink;
	if (llink->rlink != RB_NULL)
		llink->rlink->parent = node;
	parent = node->parent;
	llink->parent = parent;
	if (parent != RB_NULL) {
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

static rb_node *
node_new(void *key, void *datum)
{
	rb_node *node;

	if ((node = MALLOC(sizeof(*node))) == NULL)
		return NULL;

	node->key = key;
	node->datum = datum;
	node->color = RB_RED;
	node->llink = RB_NULL;
	node->rlink = RB_NULL;

	return node;
}

static rb_node *
node_next(rb_node *node)
{
	rb_node *temp;

	ASSERT(node != NULL);

	if (node->rlink != RB_NULL) {
		for (node = node->rlink; node->llink != RB_NULL; node = node->llink)
			/* void */;
	} else {
		temp = node->parent;
		while (temp != RB_NULL && temp->rlink == node) {
			node = temp;
			temp = temp->parent;
		}
		node = temp;
	}

	return node;
}

static rb_node *
node_prev(rb_node *node)
{
	rb_node *temp;

	ASSERT(node != NULL);

	if (node->llink != RB_NULL) {
		for (node = node->llink; node->rlink != RB_NULL; node = node->rlink)
			/* void */;
	} else {
		temp = node->parent;
		while (temp != RB_NULL && temp->llink == node) {
			node = temp;
			temp = temp->parent;
		}
		node = temp;
	}

	return node;
}

static rb_node *
node_max(rb_node *node)
{
	ASSERT(node != NULL);

	while (node->rlink != RB_NULL)
		node = node->rlink;
	return node;
}

static rb_node *
node_min(rb_node *node)
{
	ASSERT(node != NULL);

	while (node->llink != RB_NULL)
		node = node->llink;
	return node;
}

rb_itor *
rb_itor_new(rb_tree *tree)
{
	rb_itor *itor;

	ASSERT(tree != NULL);

	if ((itor = MALLOC(sizeof(*itor))) == NULL)
		return NULL;
	itor->tree = tree;
	rb_itor_first(itor);
	return itor;
}

dict_itor *
rb_dict_itor_new(rb_tree *tree)
{
	dict_itor *itor;

	ASSERT(tree != NULL);

	if ((itor = MALLOC(sizeof(*itor))) == NULL)
		return NULL;

	if ((itor->_itor = rb_itor_new(tree)) == NULL) {
		FREE(itor);
		return NULL;
	}

	itor->_vtable = &rb_tree_itor_vtable;

	return itor;
}

void
rb_itor_destroy(rb_itor *itor)
{
	ASSERT(itor != NULL);

	FREE(itor);
}

#define RETVALID(itor)		return itor->node != RB_NULL

int
rb_itor_valid(const rb_itor *itor)
{
	ASSERT(itor != NULL);

	RETVALID(itor);
}

void
rb_itor_invalidate(rb_itor *itor)
{
	ASSERT(itor != NULL);

	itor->node = RB_NULL;
}

int
rb_itor_next(rb_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->node == RB_NULL)
		rb_itor_first(itor);
	else
		itor->node = node_next(itor->node);
	RETVALID(itor);
}

int
rb_itor_prev(rb_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->node == RB_NULL)
		rb_itor_last(itor);
	else
		itor->node = node_prev(itor->node);
	RETVALID(itor);
}

int
rb_itor_nextn(rb_itor *itor, unsigned count)
{
	ASSERT(itor != NULL);

	if (count) {
		if (itor->node == RB_NULL) {
			rb_itor_first(itor);
			count--;
		}

		while (count-- && itor->node)
			itor->node = node_next(itor->node);
	}

	RETVALID(itor);
}

int
rb_itor_prevn(rb_itor *itor, unsigned count)
{
	ASSERT(itor != NULL);

	if (count) {
		if (itor->node == RB_NULL) {
			rb_itor_last(itor);
			count--;
		}

		while (count-- && itor->node)
			itor->node = node_prev(itor->node);
	}

	RETVALID(itor);
}

int
rb_itor_first(rb_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->tree->root == RB_NULL)
		itor->node = RB_NULL;
	else
		itor->node = node_min(itor->tree->root);
	RETVALID(itor);
}

int
rb_itor_last(rb_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->tree->root == RB_NULL)
		itor->node = RB_NULL;
	else
		itor->node = node_max(itor->tree->root);
	RETVALID(itor);
}

int
rb_itor_search(rb_itor *itor, const void *key)
{
	rb_node *node;
	dict_compare_func cmp_func;

	ASSERT(itor != NULL);

	cmp_func = itor->tree->key_cmp;
	for (node = itor->tree->root; node != RB_NULL;) {
		int cmp = cmp_func(key, node->key);
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
rb_itor_key(const rb_itor *itor)
{
	ASSERT(itor != NULL);

	return itor->node != RB_NULL ? itor->node->key : NULL;
}

void *
rb_itor_data(rb_itor *itor)
{
	ASSERT(itor != NULL);

	return itor->node != RB_NULL ? itor->node->datum : NULL;
}

const void *
rb_itor_cdata(const rb_itor *itor)
{
	ASSERT(itor != NULL);

	return itor->node != RB_NULL ? itor->node->datum : NULL;
}

int
rb_itor_set_data(rb_itor *itor, void *datum, void **old_datum)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		return -1;

	if (old_datum)
		*old_datum = itor->node->datum;
	itor->node->datum = datum;
	return 0;
}
