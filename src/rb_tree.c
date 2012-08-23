/*
 * libdict -- red-black tree implementation.
 * cf. [Cormen, Leiserson, and Rivest 1990], [Guibas and Sedgewick, 1978]
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

#include "rb_tree.h"
#include "dict_private.h"

typedef struct rb_node rb_node;
struct rb_node {
    void*		    key;
    void*		    datum;
    rb_node*		    parent;
    rb_node*		    llink;
    rb_node*		    rlink;
    unsigned		    color:1; /* TODO: store in unused low bits. */
};

#define RB_RED		    0
#define RB_BLK		    1

struct rb_tree {
    rb_node*		    root;
    size_t		    count;
    dict_compare_func	    cmp_func;
    dict_delete_func	    del_func;
};

struct rb_itor {
    rb_tree*		    tree;
    rb_node*		    node;
};

static dict_vtable rb_tree_vtable = {
    (dict_inew_func)	    rb_dict_itor_new,
    (dict_dfree_func)	    rb_tree_free,
    (dict_insert_func)	    rb_tree_insert,
    (dict_probe_func)	    rb_tree_probe,
    (dict_search_func)	    rb_tree_search,
    (dict_remove_func)	    rb_tree_remove,
    (dict_clear_func)	    rb_tree_clear,
    (dict_traverse_func)    rb_tree_traverse,
    (dict_count_func)	    rb_tree_count
};

static itor_vtable rb_tree_itor_vtable = {
    (dict_ifree_func)	    rb_itor_free,
    (dict_valid_func)	    rb_itor_valid,
    (dict_invalidate_func)  rb_itor_invalidate,
    (dict_next_func)	    rb_itor_next,
    (dict_prev_func)	    rb_itor_prev,
    (dict_nextn_func)	    rb_itor_nextn,
    (dict_prevn_func)	    rb_itor_prevn,
    (dict_first_func)	    rb_itor_first,
    (dict_last_func)	    rb_itor_last,
    (dict_key_func)	    rb_itor_key,
    (dict_data_func)	    rb_itor_data,
    (dict_dataset_func)	    rb_itor_set_data,
    (dict_iremove_func)	    NULL,/* rb_itor_remove not implemented yet */
    (dict_icompare_func)    NULL /* rb_itor_compare not implemented yet */
};

static rb_node _null = { NULL, NULL, NULL, NULL, NULL, RB_BLK };
#define RB_NULL	&_null

static void	rot_left(rb_tree *tree, rb_node *node);
static void	rot_right(rb_tree *tree, rb_node *node);
static void	insert_fixup(rb_tree *tree, rb_node *node);
static void	delete_fixup(rb_tree *tree, rb_node *node);
static size_t	node_height(const rb_node *node);
static size_t	node_mheight(const rb_node *node);
static size_t	node_pathlen(const rb_node *node, size_t level);
static rb_node*	node_new(void *key, void *datum);
static rb_node*	node_next(rb_node *node);
static rb_node*	node_prev(rb_node *node);
static rb_node*	node_max(rb_node *node);
static rb_node*	node_min(rb_node *node);

rb_tree *
rb_tree_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
    rb_tree *tree = MALLOC(sizeof(*tree));
    if (tree) {
	tree->root = RB_NULL;
	tree->count = 0;
	tree->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	tree->del_func = del_func;
    }
    return tree;
}

dict *
rb_dict_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
    dict *dct = MALLOC(sizeof(*dct));
    if (dct) {
	if (!(dct->_object = rb_tree_new(cmp_func, del_func))) {
	    FREE(dct);
	    return NULL;
	}
	dct->_vtable = &rb_tree_vtable;
    }
    return dct;
}

size_t
rb_tree_free(rb_tree *tree)
{
    ASSERT(tree != NULL);

    size_t count = rb_tree_clear(tree);
    FREE(tree);
    return count;
}

void *
rb_tree_search(rb_tree *tree, const void *key)
{
    ASSERT(tree != NULL);

    rb_node *node = tree->root;
    while (node != RB_NULL) {
	int cmp = tree->cmp_func(key, node->key);
	if (!cmp)
	    return node->datum;
	node = (cmp < 0) ? node->llink : node->rlink;
    }
    return NULL;
}

int
rb_tree_insert(rb_tree *tree, void *key, void *datum, bool overwrite)
{
    ASSERT(tree != NULL);

    int cmp = 0;	/* Quell GCC warning about uninitialized usage. */
    rb_node *node = tree->root, *parent = RB_NULL;
    while (node != RB_NULL) {
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

    if (!(node = node_new(key, datum)))
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
    ASSERT(tree != NULL);

    int cmp = 0;
    rb_node *node = tree->root, *parent = RB_NULL;
    while (node != RB_NULL) {
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

    if (!(node = node_new(key, *datum)))
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
    ASSERT(tree != NULL);
    ASSERT(node != NULL);

    while (node != tree->root && node->parent->color == RB_RED) {
	if (node->parent == node->parent->parent->llink) {
	    rb_node *temp = node->parent->parent->rlink;
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
	    rb_node *temp = node->parent->parent->llink;
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

bool
rb_tree_remove(rb_tree *tree, const void *key)
{
    ASSERT(tree != NULL);

    rb_node *node = tree->root;
    while (node != RB_NULL) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp > 0)
	    node = node->rlink;
	else
	    break;
    }
    if (node == RB_NULL)
	return false;

    rb_node *out;
    if (node->llink == RB_NULL || node->rlink == RB_NULL) {
	out = node;
    } else {
	void *tmp;
	for (out = node->rlink; out->llink != RB_NULL; out = out->llink)
	    /* void */;
	SWAP(node->key, out->key, tmp);
	SWAP(node->datum, out->datum, tmp);
    }

    rb_node *temp = out->llink != RB_NULL ? out->llink : out->rlink;
    rb_node *parent = out->parent;
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

    return true;
}

static void
delete_fixup(rb_tree *tree, rb_node *node)
{
    ASSERT(tree != NULL);
    ASSERT(node != NULL);

    while (node != tree->root && node->color == RB_BLK) {
	if (node->parent->llink == node) {
	    rb_node *temp = node->parent->rlink;
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
	    rb_node *temp = node->parent->llink;
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

size_t
rb_tree_clear(rb_tree *tree)
{
    ASSERT(tree != NULL);

    const size_t count = tree->count;
    rb_node *node = tree->root;
    while (node != RB_NULL) {
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

	rb_node *parent = node->parent;
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

size_t
rb_tree_count(const rb_tree *tree)
{
    ASSERT(tree != NULL);

    return tree->count;
}

size_t
rb_tree_height(const rb_tree *tree)
{
    ASSERT(tree != NULL);

    return tree->root != RB_NULL ? node_height(tree->root) : 0;
}

size_t
rb_tree_mheight(const rb_tree *tree)
{
    ASSERT(tree != NULL);

    return tree->root != RB_NULL ? node_mheight(tree->root) : 0;
}

size_t
rb_tree_pathlen(const rb_tree *tree)
{
    ASSERT(tree != NULL);

    return tree->root != RB_NULL ? node_pathlen(tree->root, 1) : 0;
}

const void *
rb_tree_min(const rb_tree *tree)
{
    ASSERT(tree != NULL);

    if (tree->root == RB_NULL)
	return NULL;

    const rb_node *node = tree->root;
    for (; node->llink != RB_NULL; node = node->llink)
	/* void */;
    return node->key;
}

const void *
rb_tree_max(const rb_tree *tree)
{
    ASSERT(tree != NULL);

    if (tree->root == RB_NULL)
	return NULL;

    const rb_node *node = tree->root;
    for (; node->rlink != RB_NULL; node = node->rlink)
	/* void */;
    return node->key;
}

size_t
rb_tree_traverse(rb_tree *tree, dict_visit_func visit)
{
    ASSERT(tree != NULL);
    ASSERT(visit != NULL);

    if (tree->root == RB_NULL)
	return 0;

    size_t count = 0;
    rb_node *node = node_min(tree->root);
    for (; node != RB_NULL; node = node_next(node)) {
	++count;
	if (!visit(node->key, node->datum))
	    break;
    }
    return count;
}

static size_t
node_height(const rb_node *node)
{
    size_t l = node->llink != RB_NULL ? node_height(node->llink) + 1 : 0;
    size_t r = node->rlink != RB_NULL ? node_height(node->rlink) + 1 : 0;
    return MAX(l, r);
}

static size_t
node_mheight(const rb_node *node)
{
    size_t l = node->llink != RB_NULL ? node_mheight(node->llink) + 1 : 0;
    size_t r = node->rlink != RB_NULL ? node_mheight(node->rlink) + 1 : 0;
    return MIN(l, r);
}

static size_t
node_pathlen(const rb_node *node, size_t level)
{
    ASSERT(node != RB_NULL);

    size_t n = 0;
    if (node->llink != RB_NULL)
	n += level + node_pathlen(node->llink, level + 1);
    if (node->rlink != RB_NULL)
	n += level + node_pathlen(node->rlink, level + 1);
    return n;
}

static void
rot_left(rb_tree *tree, rb_node *node)
{
    ASSERT(tree != NULL);
    ASSERT(node != NULL);

    rb_node *rlink = node->rlink;
    node->rlink = rlink->llink;
    if (rlink->llink != RB_NULL)
	rlink->llink->parent = node;
    rb_node *parent = node->parent;
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
    ASSERT(tree != NULL);
    ASSERT(node != NULL);

    rb_node *llink = node->llink;
    node->llink = llink->rlink;
    if (llink->rlink != RB_NULL)
	llink->rlink->parent = node;
    rb_node *parent = node->parent;
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
    rb_node *node = MALLOC(sizeof(*node));
    if (node) {
	node->key = key;
	node->datum = datum;
	node->color = RB_RED;
	node->llink = RB_NULL;
	node->rlink = RB_NULL;
    }
    return node;
}

static rb_node *
node_next(rb_node *node)
{
    ASSERT(node != NULL);

    if (node->rlink != RB_NULL) {
	for (node = node->rlink; node->llink != RB_NULL; node = node->llink)
	    /* void */;
    } else {
	rb_node *temp = node->parent;
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
    ASSERT(node != NULL);

    if (node->llink != RB_NULL) {
	for (node = node->llink; node->rlink != RB_NULL; node = node->rlink)
	    /* void */;
    } else {
	rb_node *temp = node->parent;
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
    ASSERT(tree != NULL);

    rb_itor *itor = MALLOC(sizeof(*itor));
    if (itor) {
	itor->tree = tree;
	rb_itor_first(itor);
    }
    return itor;
}

dict_itor *
rb_dict_itor_new(rb_tree *tree)
{
    ASSERT(tree != NULL);

    dict_itor *itor = MALLOC(sizeof(*itor));
    if (itor) {
	if (!(itor->_itor = rb_itor_new(tree))) {
	    FREE(itor);
	    return NULL;
	}
	itor->_vtable = &rb_tree_itor_vtable;
    }
    return itor;
}

void
rb_itor_free(rb_itor *itor)
{
    ASSERT(itor != NULL);

    FREE(itor);
}

bool
rb_itor_valid(const rb_itor *itor)
{
    ASSERT(itor != NULL);

    return itor->node != RB_NULL;
}

void
rb_itor_invalidate(rb_itor *itor)
{
    ASSERT(itor != NULL);

    itor->node = RB_NULL;
}

bool
rb_itor_next(rb_itor *itor)
{
    ASSERT(itor != NULL);

    if (itor->node == RB_NULL)
	rb_itor_first(itor);
    else
	itor->node = node_next(itor->node);
    return itor->node != RB_NULL;
}

bool
rb_itor_prev(rb_itor *itor)
{
    ASSERT(itor != NULL);

    if (itor->node == RB_NULL)
	rb_itor_last(itor);
    else
	itor->node = node_prev(itor->node);
    return itor->node != RB_NULL;
}

bool
rb_itor_nextn(rb_itor *itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!rb_itor_next(itor))
	    return false;
    return itor->node != RB_NULL;
}

bool
rb_itor_prevn(rb_itor *itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!rb_itor_prev(itor))
	    return false;
    return itor->node != RB_NULL;
}

bool
rb_itor_first(rb_itor *itor)
{
    ASSERT(itor != NULL);

    if (itor->tree->root == RB_NULL)
	itor->node = RB_NULL;
    else
	itor->node = node_min(itor->tree->root);
    return itor->node != RB_NULL;
}

bool
rb_itor_last(rb_itor *itor)
{
    ASSERT(itor != NULL);

    if (itor->tree->root == RB_NULL)
	itor->node = RB_NULL;
    else
	itor->node = node_max(itor->tree->root);
    return itor->node != RB_NULL;
}

bool
rb_itor_search(rb_itor *itor, const void *key)
{
    ASSERT(itor != NULL);

    rb_node *node = itor->tree->root;
    while (node != RB_NULL) {
	int cmp = itor->tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp > 0)
	    node = node->rlink;
	else {
	    itor->node = node;
	    return true;
	}
    }
    itor->node = RB_NULL;
    return false;
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

bool
rb_itor_set_data(rb_itor *itor, void *datum, void **old_datum)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	return false;

    if (old_datum)
	*old_datum = itor->node->datum;
    itor->node->datum = datum;
    return true;
}
