/*
 * libdict -- red-black tree implementation.
 *
 * Copyright (c) 2001-2014, Farooq Mela
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * cf. [Cormen, Leiserson, and Rivest 1990], [Guibas and Sedgewick, 1978]
 */

#include "rb_tree.h"

#include <string.h>
#include "dict_private.h"
#include "tree_common.h"

typedef struct rb_node rb_node;
struct rb_node {
    void*	    key;
    void*	    datum;
    rb_node*	    parent;
    rb_node*	    llink;
    union {
	intptr_t    color;
	rb_node*    rlink;
    };
};

#define RB_RED		    0
#define RB_BLACK	    1

#define RLINK(node)	    ((rb_node*)((node)->color & ~RB_BLACK))
#define COLOR(node)	    ((node)->color & RB_BLACK)

#define SET_RED(node)	    (node)->color &= (~(intptr_t)RB_BLACK)
#define SET_BLACK(node)	    (node)->color |= ((intptr_t)RB_BLACK)
#define SET_RLINK(node,r)   (node)->color = COLOR(node) | (intptr_t)(r)

struct rb_tree {
    TREE_FIELDS(rb_node);
};

struct rb_itor {
    TREE_ITERATOR_FIELDS(rb_tree, rb_node);
};

static dict_vtable rb_tree_vtable = {
    (dict_inew_func)	    rb_dict_itor_new,
    (dict_dfree_func)	    rb_tree_free,
    (dict_insert_func)	    rb_tree_insert,
    (dict_search_func)	    rb_tree_search,
    (dict_search_func)	    rb_tree_search_le,
    (dict_search_func)	    rb_tree_search_lt,
    (dict_search_func)	    rb_tree_search_ge,
    (dict_search_func)	    rb_tree_search_gt,
    (dict_remove_func)	    rb_tree_remove,
    (dict_clear_func)	    rb_tree_clear,
    (dict_traverse_func)    rb_tree_traverse,
    (dict_count_func)	    tree_count,
    (dict_verify_func)	    rb_tree_verify,
    (dict_clone_func)	    rb_tree_clone,
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
    (dict_isearch_func)	    rb_itor_search,
    (dict_isearch_func)	    rb_itor_search_le,
    (dict_isearch_func)	    rb_itor_search_lt,
    (dict_isearch_func)	    rb_itor_search_ge,
    (dict_isearch_func)	    rb_itor_search_gt,
    (dict_iremove_func)	    NULL,/* rb_itor_remove not implemented yet */
    (dict_icompare_func)    NULL /* rb_itor_compare not implemented yet */
};

static rb_node rb_null = { NULL, NULL, NULL, NULL, { RB_BLACK } };
#define RB_NULL	&rb_null

static void	rot_left(rb_tree* tree, rb_node* node);
static void	rot_right(rb_tree* tree, rb_node* node);
static unsigned	insert_fixup(rb_tree* tree, rb_node* node);
static unsigned	delete_fixup(rb_tree* tree, rb_node* node);
static size_t	node_height(const rb_node* node);
static size_t	node_mheight(const rb_node* node);
static size_t	node_pathlen(const rb_node* node, size_t level);
static rb_node*	node_new(void* key);
static rb_node*	node_next(rb_node* node);
static rb_node*	node_prev(rb_node* node);
static rb_node*	node_max(rb_node* node);
static rb_node*	node_min(rb_node* node);

rb_tree*
rb_tree_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
    rb_tree* tree = MALLOC(sizeof(*tree));
    if (tree) {
	tree->root = RB_NULL;
	tree->count = 0;
	tree->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	tree->del_func = del_func;
	tree->rotation_count = 0;
    }
    return tree;
}

dict*
rb_dict_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
    dict* dct = MALLOC(sizeof(*dct));
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
rb_tree_free(rb_tree* tree)
{
    ASSERT(tree != NULL);

    size_t count = rb_tree_clear(tree);
    FREE(tree);
    return count;
}

static rb_node*
node_clone(rb_node* node, rb_node* parent, dict_key_datum_clone_func clone_func)
{
    if (node == RB_NULL)
	return RB_NULL;
    rb_node* clone = MALLOC(sizeof(*clone));
    if (!clone)
	return RB_NULL;
    clone->parent = parent;
    clone->key = node->key;
    clone->datum = node->datum;
    clone->llink = node_clone(node->llink, clone, clone_func);
    clone->rlink = node_clone(RLINK(node), clone, clone_func);
    if (COLOR(node) == RB_BLACK)
	SET_BLACK(clone);
    else
	SET_RED(clone);
    return clone;
}

rb_tree*
rb_tree_clone(rb_tree* tree, dict_key_datum_clone_func clone_func)
{
    ASSERT(tree != NULL);

    rb_tree* clone = rb_tree_new(tree->cmp_func, tree->del_func);
    if (clone) {
	memcpy(clone, tree, sizeof(rb_tree));
	clone->root = node_clone(tree->root, RB_NULL, clone_func);
    }
    return clone;
}

rb_node*
rb_tree_search_node(rb_tree* tree, const void* key)
{
    ASSERT(tree != NULL);
    rb_node* node = tree->root;
    while (node != RB_NULL) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp)
	    node = RLINK(node);
	else
	    return node;
    }
    return RB_NULL;
}

void*
rb_tree_search(rb_tree* tree, const void* key)
{
    ASSERT(tree != NULL);

    rb_node* node = rb_tree_search_node(tree, key);
    return (node != RB_NULL) ? node->datum : NULL;
}

rb_node*
rb_tree_search_le_node(rb_tree* tree, const void* key)
{
    ASSERT(tree != NULL);

    rb_node* node = tree->root, *ret = RB_NULL;
    while (node != RB_NULL) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp == 0) {
	    return node;
	} else if (cmp < 0) {
	    node = node->llink;
	} else {
	    ret = node;
	    node = RLINK(node);
	}
    }
    return ret;
}

void*
rb_tree_search_le(rb_tree* tree, const void* key)
{
    rb_node* node = rb_tree_search_le_node(tree, key);

    return (node != RB_NULL) ? node->datum : NULL;
}

rb_node*
rb_tree_search_lt_node(rb_tree* tree, const void* key)
{
    ASSERT(tree != NULL);

    rb_node* node = tree->root, *ret = RB_NULL;
    while (node != RB_NULL) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp <= 0) {
	    node = node->llink;
	} else {
	    ret = node;
	    node = RLINK(node);
	}
    }
    return ret;
}

void*
rb_tree_search_lt(rb_tree* tree, const void* key)
{
    rb_node* node = rb_tree_search_lt_node(tree, key);

    return (node != RB_NULL) ? node->datum : NULL;
}

rb_node*
rb_tree_search_ge_node(rb_tree* tree, const void* key)
{
    ASSERT(tree != NULL);

    rb_node* node = tree->root, *ret = RB_NULL;
    while (node != RB_NULL) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp == 0) {
	    return node;
	}
	if (cmp < 0) {
	    ret = node;
	    node = node->llink;
	} else {
	    node = RLINK(node);
	}
    }
    return ret;
}

void*
rb_tree_search_ge(rb_tree* tree, const void* key)
{
    rb_node* node = rb_tree_search_ge_node(tree, key);

    return (node != RB_NULL) ? node->datum : NULL;
}

rb_node*
rb_tree_search_gt_node(rb_tree* tree, const void* key)
{
    ASSERT(tree != NULL);

    rb_node* node = tree->root, *ret = RB_NULL;
    while (node != RB_NULL) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0) {
	    ret = node;
	    node = node->llink;
	} else {
	    node = RLINK(node);
	}
    }
    return ret;
}

void*
rb_tree_search_gt(rb_tree* tree, const void* key)
{
    rb_node* node = rb_tree_search_gt_node(tree, key);

    return (node != RB_NULL) ? node->datum : NULL;
}

void**
rb_tree_insert(rb_tree* tree, void* key, bool* inserted)
{
    ASSERT(tree != NULL);

    int cmp = 0;	/* Quell GCC warning about uninitialized usage. */
    rb_node* node = tree->root;
    rb_node* parent = RB_NULL;
    while (node != RB_NULL) {
	cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    parent = node, node = node->llink;
	else if (cmp)
	    parent = node, node = RLINK(node);
	else {
	    if (inserted)
		*inserted = false;
	    return &node->datum;
	}
    }

    if (!(node = node_new(key))) {
	return NULL;
    }
    if (inserted)
	*inserted = true;
    if ((node->parent = parent) == RB_NULL) {
	tree->root = node;
	ASSERT(tree->count == 0);
	SET_BLACK(node);
    } else {
	if (cmp < 0)
	    parent->llink = node;
	else
	    SET_RLINK(parent, node);

	tree->rotation_count += insert_fixup(tree, node);
    }
    ++tree->count;
    return &node->datum;
}

static unsigned
insert_fixup(rb_tree* tree, rb_node* node)
{
    ASSERT(tree != NULL);
    ASSERT(node != NULL);

    unsigned rotations = 0;
    while (node != tree->root && COLOR(node->parent) == RB_RED) {
	if (node->parent == node->parent->parent->llink) {
	    rb_node* temp = RLINK(node->parent->parent);
	    if (COLOR(temp) == RB_RED) {
		SET_BLACK(temp);
		node = node->parent;
		SET_BLACK(node);
		node = node->parent;
		SET_RED(node);
	    } else {
		if (node == RLINK(node->parent)) {
		    node = node->parent;
		    rot_left(tree, node);
		    ++rotations;
		}
		temp = node->parent;
		SET_BLACK(temp);
		temp = temp->parent;
		SET_RED(temp);
		rot_right(tree, temp);
		++rotations;
	    }
	} else {
	    rb_node* temp = node->parent->parent->llink;
	    if (COLOR(temp) == RB_RED) {
		SET_BLACK(temp);
		node = node->parent;
		SET_BLACK(node);
		node = node->parent;
		SET_RED(node);
	    } else {
		if (node == node->parent->llink) {
		    node = node->parent;
		    rot_right(tree, node);
		    ++rotations;
		}
		temp = node->parent;
		SET_BLACK(temp);
		temp = temp->parent;
		SET_RED(temp);
		rot_left(tree, temp);
		++rotations;
	    }
	}
    }

    SET_BLACK(tree->root);
    return rotations;
}

bool
rb_tree_remove(rb_tree* tree, const void* key)
{
    ASSERT(tree != NULL);

    rb_node* node = tree->root;
    while (node != RB_NULL) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp)
	    node = RLINK(node);
	else
	    break;
    }
    if (node == RB_NULL)
	return false;

    rb_node* out;
    if (node->llink == RB_NULL || RLINK(node) == RB_NULL) {
	out = node;
    } else {
	void* tmp;
	for (out = RLINK(node); out->llink != RB_NULL; out = out->llink)
	    /* void */;
	SWAP(node->key, out->key, tmp);
	SWAP(node->datum, out->datum, tmp);
    }

    rb_node* temp = out->llink != RB_NULL ? out->llink : RLINK(out);
    rb_node* parent = out->parent;
    temp->parent = parent;
    if (parent != RB_NULL) {
	if (parent->llink == out)
	    parent->llink = temp;
	else
	    SET_RLINK(parent, temp);
    } else {
	tree->root = temp;
    }

    if (COLOR(out) == RB_BLACK)
	tree->rotation_count += delete_fixup(tree, temp);
    if (tree->del_func)
	tree->del_func(out->key, out->datum);
    FREE(out);

    tree->count--;

    return true;
}

static unsigned
delete_fixup(rb_tree* tree, rb_node* node)
{
    ASSERT(tree != NULL);
    ASSERT(node != NULL);

    unsigned rotations = 0;
    while (node != tree->root && COLOR(node) == RB_BLACK) {
	if (node->parent->llink == node) {
	    rb_node* temp = RLINK(node->parent);
	    if (COLOR(temp) == RB_RED) {
		SET_BLACK(temp);
		SET_RED(node->parent);
		rot_left(tree, node->parent);
		++rotations;
		temp = RLINK(node->parent);
	    }
	    if (COLOR(temp->llink) == RB_BLACK &&
		COLOR(RLINK(temp)) == RB_BLACK) {
		SET_RED(temp);
		node = node->parent;
	    } else {
		if (COLOR(RLINK(temp)) == RB_BLACK) {
		    SET_BLACK(temp->llink);
		    SET_RED(temp);
		    rot_right(tree, temp);
		    ++rotations;
		    temp = RLINK(node->parent);
		}
		if (COLOR(node->parent) == RB_RED)
		    SET_RED(temp);
		else
		    SET_BLACK(temp);
		SET_BLACK(RLINK(temp));
		SET_BLACK(node->parent);
		rot_left(tree, node->parent);
		++rotations;
		break;
	    }
	} else {
	    rb_node* temp = node->parent->llink;
	    if (COLOR(temp) == RB_RED) {
		SET_BLACK(temp);
		SET_RED(node->parent);
		rot_right(tree, node->parent);
		++rotations;
		temp = node->parent->llink;
	    }
	    if (COLOR(RLINK(temp)) == RB_BLACK &&
		COLOR(temp->llink) == RB_BLACK) {
		SET_RED(temp);
		node = node->parent;
	    } else {
		if (COLOR(temp->llink) == RB_BLACK) {
		    SET_BLACK(RLINK(temp));
		    SET_RED(temp);
		    rot_left(tree, temp);
		    ++rotations;
		    temp = node->parent->llink;
		}
		if (COLOR(node->parent) == RB_RED)
		    SET_RED(temp);
		else
		    SET_BLACK(temp);
		SET_BLACK(node->parent);
		SET_BLACK(temp->llink);
		rot_right(tree, node->parent);
		++rotations;
		break;
	    }
	}
    }

    SET_BLACK(node);
    return rotations;
}

size_t
rb_tree_clear(rb_tree* tree)
{
    ASSERT(tree != NULL);

    const size_t count = tree->count;
    rb_node* node = tree->root;
    while (node != RB_NULL) {
	if (node->llink != RB_NULL) {
	    node = node->llink;
	    continue;
	}
	if (RLINK(node) != RB_NULL) {
	    node = RLINK(node);
	    continue;
	}

	if (tree->del_func)
	    tree->del_func(node->key, node->datum);

	rb_node* parent = node->parent;
	FREE(node);
	tree->count--;
	if (parent != RB_NULL) {
	    if (parent->llink == node)
		parent->llink = RB_NULL;
	    else
		SET_RLINK(parent, RB_NULL);
	}
	node = parent;
    }

    tree->root = RB_NULL;
    ASSERT(tree->count == 0);
    return count;
}

size_t
rb_tree_count(const rb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree_count(tree);
}

size_t
rb_tree_height(const rb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root != RB_NULL ? node_height(tree->root) : 0;
}

size_t
rb_tree_mheight(const rb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root != RB_NULL ? node_mheight(tree->root) : 0;
}

size_t
rb_tree_pathlen(const rb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root != RB_NULL ? node_pathlen(tree->root, 1) : 0;
}

const void*
rb_tree_min(const rb_tree* tree)
{
    ASSERT(tree != NULL);

    if (tree->root == RB_NULL)
	return NULL;

    const rb_node* node = tree->root;
    for (; node->llink != RB_NULL; node = node->llink)
	/* void */;
    return node->key;
}

const void*
rb_tree_max(const rb_tree* tree)
{
    ASSERT(tree != NULL);

    if (tree->root == RB_NULL)
	return NULL;

    const rb_node* node = tree->root;
    for (; RLINK(node) != RB_NULL; node = RLINK(node))
	/* void */;
    return node->key;
}

size_t
rb_tree_traverse(rb_tree* tree, dict_visit_func visit)
{
    ASSERT(tree != NULL);
    ASSERT(visit != NULL);

    if (tree->root == RB_NULL)
	return 0;

    size_t count = 0;
    rb_node* node = node_min(tree->root);
    for (; node != RB_NULL; node = node_next(node)) {
	++count;
	if (!visit(node->key, node->datum))
	    break;
    }
    return count;
}

static size_t
node_height(const rb_node* node)
{
    size_t l = node->llink != RB_NULL ? node_height(node->llink) + 1 : 0;
    size_t r = RLINK(node) != RB_NULL ? node_height(RLINK(node)) + 1 : 0;
    return MAX(l, r);
}

static size_t
node_mheight(const rb_node* node)
{
    size_t l = node->llink != RB_NULL ? node_mheight(node->llink) + 1 : 0;
    size_t r = RLINK(node) != RB_NULL ? node_mheight(RLINK(node)) + 1 : 0;
    return MIN(l, r);
}

static size_t
node_pathlen(const rb_node* node, size_t level)
{
    ASSERT(node != RB_NULL);

    size_t n = 0;
    if (node->llink != RB_NULL)
	n += level + node_pathlen(node->llink, level + 1);
    if (RLINK(node) != RB_NULL)
	n += level + node_pathlen(RLINK(node), level + 1);
    return n;
}

static void
rot_left(rb_tree* tree, rb_node* node)
{
    ASSERT(tree != NULL);
    ASSERT(node != NULL);

    rb_node* rlink = RLINK(node);
    SET_RLINK(node, rlink->llink);
    if (rlink->llink != RB_NULL)
	rlink->llink->parent = node;
    rb_node* parent = node->parent;
    rlink->parent = parent;
    if (parent != RB_NULL) {
	if (parent->llink == node)
	    parent->llink = rlink;
	else
	    SET_RLINK(parent, rlink);
    } else {
	tree->root = rlink;
    }
    rlink->llink = node;
    node->parent = rlink;
}

static void
rot_right(rb_tree* tree, rb_node* node)
{
    ASSERT(tree != NULL);
    ASSERT(node != NULL);

    rb_node* llink = node->llink;
    node->llink = RLINK(llink);
    if (RLINK(llink) != RB_NULL)
	RLINK(llink)->parent = node;
    rb_node* parent = node->parent;
    llink->parent = parent;
    if (parent != RB_NULL) {
	if (parent->llink == node)
	    parent->llink = llink;
	else
	    SET_RLINK(parent, llink);
    } else {
	tree->root = llink;
    }
    SET_RLINK(llink, node);
    node->parent = llink;
}

static rb_node*
node_new(void* key)
{
    rb_node* node = MALLOC(sizeof(*node));
    if (node) {
	ASSERT((((intptr_t)node) & 1) == 0); /* Ensure malloc returns aligned result. */
	node->key = key;
	node->datum = NULL;
	node->parent = RB_NULL;
	node->llink = RB_NULL;
	node->rlink = RB_NULL;
	SET_RED(node);
    }
    return node;
}

static rb_node*
node_next(rb_node* node)
{
    ASSERT(node != NULL);

    if (RLINK(node) != RB_NULL) {
	for (node = RLINK(node); node->llink != RB_NULL; node = node->llink)
	    /* void */;
    } else {
	rb_node* temp = node->parent;
	while (temp != RB_NULL && RLINK(temp) == node) {
	    node = temp;
	    temp = temp->parent;
	}
	node = temp;
    }

    return node;
}

static rb_node*
node_prev(rb_node* node)
{
    ASSERT(node != NULL);

    if (node->llink != RB_NULL) {
	for (node = node->llink; RLINK(node) != RB_NULL; node = RLINK(node))
	    /* void */;
    } else {
	rb_node* temp = node->parent;
	while (temp != RB_NULL && temp->llink == node) {
	    node = temp;
	    temp = temp->parent;
	}
	node = temp;
    }

    return node;
}

static rb_node*
node_max(rb_node* node)
{
    ASSERT(node != NULL);

    while (RLINK(node) != RB_NULL)
	node = RLINK(node);
    return node;
}

static rb_node*
node_min(rb_node* node)
{
    ASSERT(node != NULL);

    while (node->llink != RB_NULL)
	node = node->llink;
    return node;
}

static bool
node_verify(const rb_tree* tree, const rb_node* parent, const rb_node* node)
{
    ASSERT(tree != NULL);

    if (parent == RB_NULL) {
	VERIFY(tree->root == node);
	VERIFY(COLOR(node) == RB_BLACK);
    } else {
	VERIFY(parent->llink == node || RLINK(parent) == node);
    }
    if (node != RB_NULL) {
	VERIFY(node->parent == parent);
	if (COLOR(node) == RB_RED) {
	    /* Verify that every child of a red node is black. */
	    VERIFY(COLOR(node->llink) == RB_BLACK);
	    VERIFY(COLOR(node->rlink) == RB_BLACK);
	}
	if (!node_verify(tree, node, node->llink) ||
	    !node_verify(tree, node, RLINK(node)))
	    return false;
    } else {
	/* Verify that every leaf is black. */
	VERIFY(COLOR(node) == RB_BLACK);
    }
    return true;
}

bool
rb_tree_verify(const rb_tree* tree)
{
    ASSERT(tree != NULL);

    if (tree->root != RB_NULL) {
	VERIFY(tree->count > 0);
    } else {
	VERIFY(tree->count == 0);
    }
    return node_verify(tree, RB_NULL, tree->root);
}

rb_itor*
rb_itor_new(rb_tree* tree)
{
    ASSERT(tree != NULL);

    rb_itor* itor = MALLOC(sizeof(*itor));
    if (itor) {
	itor->tree = tree;
	itor->node = RB_NULL;
    }
    return itor;
}

dict_itor*
rb_dict_itor_new(rb_tree* tree)
{
    ASSERT(tree != NULL);

    dict_itor* itor = MALLOC(sizeof(*itor));
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
rb_itor_free(rb_itor* itor)
{
    ASSERT(itor != NULL);

    FREE(itor);
}

bool
rb_itor_valid(const rb_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->node != RB_NULL;
}

void
rb_itor_invalidate(rb_itor* itor)
{
    ASSERT(itor != NULL);

    itor->node = RB_NULL;
}

bool
rb_itor_next(rb_itor* itor)
{
    ASSERT(itor != NULL);

    if (itor->node == RB_NULL)
	rb_itor_first(itor);
    else
	itor->node = node_next(itor->node);
    return itor->node != RB_NULL;
}

bool
rb_itor_prev(rb_itor* itor)
{
    ASSERT(itor != NULL);

    if (itor->node == RB_NULL)
	rb_itor_last(itor);
    else
	itor->node = node_prev(itor->node);
    return itor->node != RB_NULL;
}

bool
rb_itor_nextn(rb_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!rb_itor_next(itor))
	    return false;
    return itor->node != RB_NULL;
}

bool
rb_itor_prevn(rb_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!rb_itor_prev(itor))
	    return false;
    return itor->node != RB_NULL;
}

bool
rb_itor_first(rb_itor* itor)
{
    ASSERT(itor != NULL);

    if (itor->tree->root == RB_NULL)
	itor->node = RB_NULL;
    else
	itor->node = node_min(itor->tree->root);
    return itor->node != RB_NULL;
}

bool
rb_itor_last(rb_itor* itor)
{
    ASSERT(itor != NULL);

    if (itor->tree->root == RB_NULL)
	itor->node = RB_NULL;
    else
	itor->node = node_max(itor->tree->root);
    return itor->node != RB_NULL;
}

bool
rb_itor_search(rb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);

    rb_node* node = itor->tree->root;
    while (node != RB_NULL) {
	int cmp = itor->tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp)
	    node = RLINK(node);
	else {
	    itor->node = node;
	    return true;
	}
    }
    itor->node = RB_NULL;
    return false;
}

bool
rb_itor_search_le(rb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    ASSERT(itor->tree != NULL);
    return (itor->node = rb_tree_search_le_node(itor->tree, key)) != RB_NULL;
}

bool
rb_itor_search_lt(rb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    ASSERT(itor->tree != NULL);
    return (itor->node = rb_tree_search_lt_node(itor->tree, key)) != RB_NULL;
}

bool
rb_itor_search_ge(rb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    ASSERT(itor->tree != NULL);
    return (itor->node = rb_tree_search_ge_node(itor->tree, key)) != RB_NULL;
}

bool
rb_itor_search_gt(rb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    ASSERT(itor->tree != NULL);
    return (itor->node = rb_tree_search_gt_node(itor->tree, key)) != RB_NULL;
}

const void*
rb_itor_key(const rb_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->node != RB_NULL ? itor->node->key : NULL;
}

void**
rb_itor_data(rb_itor* itor)
{
    ASSERT(itor != NULL);

    return (itor->node != RB_NULL) ? &itor->node->datum : NULL;
}
