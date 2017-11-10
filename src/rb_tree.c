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
    intptr_t        color;
    rb_node*	    llink;
    rb_node*	    rlink;
};

#define RB_RED		    0
#define RB_BLACK	    1

#define PARENT(node)	    ((rb_node*)((node)->color & ~RB_BLACK))
#define COLOR(node)	    ((node)->color & RB_BLACK)

#define SET_RED(node)	    (node)->color &= (~(intptr_t)RB_BLACK)
#define SET_BLACK(node)	    (node)->color |= ((intptr_t)RB_BLACK)
#define SET_PARENT(node,p)  (node)->color = COLOR(node) | (intptr_t)(p)

struct rb_tree {
    TREE_FIELDS(rb_node);
};

struct rb_itor {
    TREE_ITERATOR_FIELDS(rb_tree, rb_node);
};

static const dict_vtable rb_tree_vtable = {
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
    (dict_select_func)	    rb_tree_select,
    (dict_count_func)	    tree_count,
    (dict_verify_func)	    rb_tree_verify,
};

static const itor_vtable rb_tree_itor_vtable = {
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
    (dict_datum_func)	    rb_itor_datum,
    (dict_isearch_func)	    rb_itor_search,
    (dict_isearch_func)	    rb_itor_search_le,
    (dict_isearch_func)	    rb_itor_search_lt,
    (dict_isearch_func)	    rb_itor_search_ge,
    (dict_isearch_func)	    rb_itor_search_gt,
    (dict_iremove_func)	    NULL,/* rb_itor_remove not implemented yet */
    (dict_icompare_func)    NULL,/* rb_itor_compare not implemented yet */
};

static rb_node rb_null = { NULL, NULL, RB_BLACK, NULL, NULL };
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
rb_tree_new(dict_compare_func cmp_func)
{
    ASSERT(cmp_func != NULL);

    rb_tree* tree = MALLOC(sizeof(*tree));
    if (tree) {
	tree->root = RB_NULL;
	tree->count = 0;
	tree->cmp_func = cmp_func;
	tree->rotation_count = 0;
    }
    return tree;
}

dict*
rb_dict_new(dict_compare_func cmp_func)
{
    dict* dct = MALLOC(sizeof(*dct));
    if (dct) {
	if (!(dct->_object = rb_tree_new(cmp_func))) {
	    FREE(dct);
	    return NULL;
	}
	dct->_vtable = &rb_tree_vtable;
    }
    return dct;
}

size_t
rb_tree_free(rb_tree* tree, dict_delete_func delete_func)
{
    size_t count = rb_tree_clear(tree, delete_func);
    FREE(tree);
    return count;
}

static rb_node*
rb_tree_search_node(rb_tree* tree, const void* key)
{
    rb_node* node = tree->root;
    while (node != RB_NULL) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp)
	    node = node->rlink;
	else
	    return node;
    }
    return RB_NULL;
}

void**
rb_tree_search(rb_tree* tree, const void* key)
{
    rb_node* node = rb_tree_search_node(tree, key);
    return (node != RB_NULL) ? &node->datum : NULL;
}

static rb_node*
rb_tree_search_le_node(rb_tree* tree, const void* key)
{
    rb_node* node = tree->root, *ret = RB_NULL;
    while (node != RB_NULL) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp == 0) {
	    return node;
	} else if (cmp < 0) {
	    node = node->llink;
	} else {
	    ret = node;
	    node = node->rlink;
	}
    }
    return ret;
}

void**
rb_tree_search_le(rb_tree* tree, const void* key)
{
    rb_node* node = rb_tree_search_le_node(tree, key);
    return (node != RB_NULL) ? &node->datum : NULL;
}

static rb_node*
rb_tree_search_lt_node(rb_tree* tree, const void* key)
{
    rb_node* node = tree->root, *ret = RB_NULL;
    while (node != RB_NULL) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp <= 0) {
	    node = node->llink;
	} else {
	    ret = node;
	    node = node->rlink;
	}
    }
    return ret;
}

void**
rb_tree_search_lt(rb_tree* tree, const void* key)
{
    rb_node* node = rb_tree_search_lt_node(tree, key);
    return (node != RB_NULL) ? &node->datum : NULL;
}

static rb_node*
rb_tree_search_ge_node(rb_tree* tree, const void* key)
{
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
	    node = node->rlink;
	}
    }
    return ret;
}

void**
rb_tree_search_ge(rb_tree* tree, const void* key)
{
    rb_node* node = rb_tree_search_ge_node(tree, key);
    return (node != RB_NULL) ? &node->datum : NULL;
}

static rb_node*
rb_tree_search_gt_node(rb_tree* tree, const void* key)
{
    rb_node* node = tree->root, *ret = RB_NULL;
    while (node != RB_NULL) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0) {
	    ret = node;
	    node = node->llink;
	} else {
	    node = node->rlink;
	}
    }
    return ret;
}

void**
rb_tree_search_gt(rb_tree* tree, const void* key)
{
    rb_node* node = rb_tree_search_gt_node(tree, key);
    return (node != RB_NULL) ? &node->datum : NULL;
}

dict_insert_result
rb_tree_insert(rb_tree* tree, void* key)
{
    int cmp = 0;	/* Quell GCC warning about uninitialized usage. */
    rb_node* node = tree->root;
    rb_node* parent = RB_NULL;
    while (node != RB_NULL) {
	cmp = tree->cmp_func(key, node->key);
	if (cmp < 0) {
	    parent = node; node = node->llink;
	} else if (cmp > 0) {
	    parent = node; node = node->rlink;
	} else
	    return (dict_insert_result) { &node->datum, false };
    }

    if (!(node = node_new(key)))
	return (dict_insert_result) { NULL, false };

    SET_PARENT(node, parent);
    if (parent == RB_NULL) {
	ASSERT(tree->root == RB_NULL);
	tree->root = node;
	ASSERT(tree->count == 0);
	SET_BLACK(node);
    } else {
	if (cmp < 0)
	    parent->llink = node;
	else
	    parent->rlink = node;

	tree->rotation_count += insert_fixup(tree, node);
    }
    tree->count++;
    return (dict_insert_result) { &node->datum, true };
}

static unsigned
insert_fixup(rb_tree* tree, rb_node* node)
{
    unsigned rotations = 0;
    while (node != tree->root && COLOR(PARENT(node)) == RB_RED) {
	if (PARENT(node) == PARENT(PARENT(node))->llink) {
	    rb_node* temp = PARENT(PARENT(node))->rlink;
	    if (COLOR(temp) == RB_RED) {
		SET_BLACK(temp);
		node = PARENT(node);
		SET_BLACK(node);
		node = PARENT(node);
		SET_RED(node);
	    } else {
		if (node == PARENT(node)->rlink) {
		    node = PARENT(node);
		    rot_left(tree, node);
		    ++rotations;
		}
		temp = PARENT(node);
		SET_BLACK(temp);
		temp = PARENT(temp);
		SET_RED(temp);
		rot_right(tree, temp);
		++rotations;
	    }
	} else {
	    rb_node* temp = PARENT(PARENT(node))->llink;
	    if (COLOR(temp) == RB_RED) {
		SET_BLACK(temp);
		node = PARENT(node);
		SET_BLACK(node);
		node = PARENT(node);
		SET_RED(node);
	    } else {
		if (node == PARENT(node)->llink) {
		    node = PARENT(node);
		    rot_right(tree, node);
		    ++rotations;
		}
		temp = PARENT(node);
		SET_BLACK(temp);
		temp = PARENT(temp);
		SET_RED(temp);
		rot_left(tree, temp);
		++rotations;
	    }
	}
    }

    SET_BLACK(tree->root);
    return rotations;
}

dict_remove_result
rb_tree_remove(rb_tree* tree, const void* key)
{
    rb_node* node = tree->root;
    while (node != RB_NULL) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp)
	    node = node->rlink;
	else
	    break;
    }
    if (node == RB_NULL)
	return (dict_remove_result) { NULL, NULL, false };

    rb_node* out;
    if (node->llink == RB_NULL || node->rlink == RB_NULL) {
	out = node;
    } else {
	void* tmp;
	for (out = node->rlink; out->llink != RB_NULL; out = out->llink)
	    /* void */;
	SWAP(node->key, out->key, tmp);
	SWAP(node->datum, out->datum, tmp);
    }

    rb_node* const temp = out->llink != RB_NULL ? out->llink : out->rlink;
    rb_node* const parent = PARENT(out);
    SET_PARENT(temp, parent);
    *(parent != RB_NULL ? (parent->llink == out ? &parent->llink : &parent->rlink) : &tree->root) = temp;

    if (COLOR(out) == RB_BLACK)
	tree->rotation_count += delete_fixup(tree, temp);
    dict_remove_result result = { out->key, out->datum, true };
    FREE(out);
    tree->count--;
    return result;
}

static unsigned
delete_fixup(rb_tree* tree, rb_node* node)
{
    unsigned rotations = 0;
    while (node != tree->root && COLOR(node) == RB_BLACK) {
	if (PARENT(node)->llink == node) {
	    rb_node* temp = PARENT(node)->rlink;
	    if (COLOR(temp) == RB_RED) {
		SET_BLACK(temp);
		SET_RED(PARENT(node));
		rot_left(tree, PARENT(node));
		++rotations;
		temp = PARENT(node)->rlink;
	    }
	    if (COLOR(temp->llink) == RB_BLACK &&
		COLOR(temp->rlink) == RB_BLACK) {
		SET_RED(temp);
		node = PARENT(node);
	    } else {
		if (COLOR(temp->rlink) == RB_BLACK) {
		    SET_BLACK(temp->llink);
		    SET_RED(temp);
		    rot_right(tree, temp);
		    ++rotations;
		    temp = PARENT(node)->rlink;
		}
		if (COLOR(PARENT(node)) == RB_RED)
		    SET_RED(temp);
		else
		    SET_BLACK(temp);
		SET_BLACK(temp->rlink);
		SET_BLACK(PARENT(node));
		rot_left(tree, PARENT(node));
		++rotations;
		break;
	    }
	} else {
	    rb_node* temp = PARENT(node)->llink;
	    if (COLOR(temp) == RB_RED) {
		SET_BLACK(temp);
		SET_RED(PARENT(node));
		rot_right(tree, PARENT(node));
		++rotations;
		temp = PARENT(node)->llink;
	    }
	    if (COLOR(temp->rlink) == RB_BLACK &&
		COLOR(temp->llink) == RB_BLACK) {
		SET_RED(temp);
		node = PARENT(node);
	    } else {
		if (COLOR(temp->llink) == RB_BLACK) {
		    SET_BLACK(temp->rlink);
		    SET_RED(temp);
		    rot_left(tree, temp);
		    ++rotations;
		    temp = PARENT(node)->llink;
		}
		if (COLOR(PARENT(node)) == RB_RED)
		    SET_RED(temp);
		else
		    SET_BLACK(temp);
		SET_BLACK(PARENT(node));
		SET_BLACK(temp->llink);
		rot_right(tree, PARENT(node));
		++rotations;
		break;
	    }
	}
    }

    SET_BLACK(node);
    return rotations;
}

size_t
rb_tree_clear(rb_tree* tree, dict_delete_func delete_func)
{
    const size_t count = tree->count;
    rb_node* node = tree->root;
    while (node != RB_NULL) {
	if (node->llink != RB_NULL) {
	    node = node->llink;
	    continue;
	}
	if (node->rlink != RB_NULL) {
	    node = node->rlink;
	    continue;
	}

	if (delete_func)
	    delete_func(node->key, node->datum);

	rb_node* parent = PARENT(node);
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
rb_tree_count(const rb_tree* tree)
{
    return tree_count(tree);
}

size_t
rb_tree_min_path_length(const rb_tree* tree)
{
    return tree->root != RB_NULL ? node_mheight(tree->root) : 0;
}

size_t
rb_tree_max_path_length(const rb_tree* tree)
{
    return tree->root != RB_NULL ? node_height(tree->root) : 0;
}

size_t
rb_tree_total_path_length(const rb_tree* tree)
{
    return tree->root != RB_NULL ? node_pathlen(tree->root, 1) : 0;
}

const void*
rb_tree_min(const rb_tree* tree)
{
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
    if (tree->root == RB_NULL)
	return NULL;

    const rb_node* node = tree->root;
    for (; node->rlink != RB_NULL; node = node->rlink)
	/* void */;
    return node->key;
}

size_t
rb_tree_traverse(rb_tree* tree, dict_visit_func visit)
{
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

bool
rb_tree_select(rb_tree *tree, size_t n, const void **key, void **datum)
{
    if (n >= tree->count) {
	if (key)
	    *key = NULL;
	if (datum)
	    *datum = NULL;
	return false;
    }
    rb_node *node;
    if (n >= tree->count / 2) {
	node = node_max(tree->root);
	n = tree->count - 1 - n;
	while (n--)
	    node = node_prev(node);
    } else {
	node = node_min(tree->root);
	while (n--)
	    node = node_next(node);
    }
    if (key)
	*key = node->key;
    if (datum)
	*datum = node->datum;
    return true;
}


static size_t
node_height(const rb_node* node)
{
    size_t l = node->llink != RB_NULL ? node_height(node->llink) + 1 : 0;
    size_t r = node->rlink != RB_NULL ? node_height(node->rlink) + 1 : 0;
    return MAX(l, r);
}

static size_t
node_mheight(const rb_node* node)
{
    size_t l = node->llink != RB_NULL ? node_mheight(node->llink) + 1 : 0;
    size_t r = node->rlink != RB_NULL ? node_mheight(node->rlink) + 1 : 0;
    return MIN(l, r);
}

static size_t
node_pathlen(const rb_node* node, size_t level)
{
    size_t n = 0;
    if (node->llink != RB_NULL)
	n += level + node_pathlen(node->llink, level + 1);
    if (node->rlink != RB_NULL)
	n += level + node_pathlen(node->rlink, level + 1);
    return n;
}

static void
rot_left(rb_tree* tree, rb_node* node)
{
    rb_node* const rlink = node->rlink;
    if ((node->rlink = rlink->llink) != RB_NULL)
	SET_PARENT(rlink->llink, node);
    rb_node* const parent = PARENT(node);
    SET_PARENT(rlink, parent);
    *(parent != RB_NULL ? (parent->llink == node ? &parent->llink : &parent->rlink) : &tree->root) = rlink;
    rlink->llink = node;
    SET_PARENT(node, rlink);
}

static void
rot_right(rb_tree* tree, rb_node* node)
{
    rb_node* const llink = node->llink;
    if ((node->llink = llink->rlink) != RB_NULL)
	SET_PARENT(llink->rlink, node);
    rb_node* const parent = PARENT(node);
    SET_PARENT(llink, parent);
    *(parent != RB_NULL ? (parent->llink == node ? &parent->llink : &parent->rlink) : &tree->root) = llink;
    llink->rlink = node;
    SET_PARENT(node, llink);
}

static rb_node*
node_new(void* key)
{
    rb_node* node = MALLOC(sizeof(*node));
    if (node) {
	ASSERT((((intptr_t)node) & 1) == 0); /* Ensure malloc returns aligned result. */
	node->key = key;
	node->datum = NULL;
	SET_PARENT(node, RB_NULL);
	SET_RED(node);
	node->llink = RB_NULL;
	node->rlink = RB_NULL;
    }
    return node;
}

static rb_node*
node_next(rb_node* node)
{
    if (node->rlink != RB_NULL) {
	for (node = node->rlink; node->llink != RB_NULL; node = node->llink)
	    /* void */;
    } else {
	rb_node* temp = PARENT(node);
	while (temp != RB_NULL && temp->rlink == node) {
	    node = temp;
	    temp = PARENT(temp);
	}
	node = temp;
    }

    return node;
}

static rb_node*
node_prev(rb_node* node)
{
    if (node->llink != RB_NULL) {
	for (node = node->llink; node->rlink != RB_NULL; node = node->rlink)
	    /* void */;
    } else {
	rb_node* temp = PARENT(node);
	while (temp != RB_NULL && temp->llink == node) {
	    node = temp;
	    temp = PARENT(temp);
	}
	node = temp;
    }

    return node;
}

static rb_node*
node_max(rb_node* node)
{
    while (node->rlink != RB_NULL)
	node = node->rlink;
    return node;
}

static rb_node*
node_min(rb_node* node)
{
    while (node->llink != RB_NULL)
	node = node->llink;
    return node;
}

static bool
node_verify(const rb_tree* tree, const rb_node* parent, const rb_node* node,
	    unsigned black_node_count, unsigned leaf_black_node_count)
{
    if (parent == RB_NULL) {
	VERIFY(tree->root == node);
	VERIFY(COLOR(node) == RB_BLACK);
    } else {
	VERIFY(parent->llink == node || parent->rlink == node);
    }
    if (node != RB_NULL) {
	VERIFY(PARENT(node) == parent);
	if (COLOR(node) == RB_RED) {
	    /* Verify that every child of a red node is black. */
	    VERIFY(COLOR(node->llink) == RB_BLACK);
	    VERIFY(COLOR(node->rlink) == RB_BLACK);
	} else {
	    black_node_count++;
	}
	if (node->llink == RB_NULL && node->rlink == RB_NULL) {
	    /* Verify that each path to a leaf contains the same number of black nodes. */
	    VERIFY(black_node_count == leaf_black_node_count);
	}
	bool l = node_verify(tree, node, node->llink, black_node_count, leaf_black_node_count);
	bool r = node_verify(tree, node, node->rlink, black_node_count, leaf_black_node_count);
	return l && r;
    } else {
	VERIFY(COLOR(node) == RB_BLACK);
    }
    return true;
}

bool
rb_tree_verify(const rb_tree* tree)
{
    VERIFY(COLOR(tree->root) == RB_BLACK);
    unsigned leaf_black_node_count = 0;
    if (tree->root != RB_NULL) {
	VERIFY(tree->count > 0);
	for (rb_node* node = tree->root; node != RB_NULL; node = node->llink)
	    if (COLOR(node) == RB_BLACK)
		leaf_black_node_count++;
    } else {
	VERIFY(tree->count == 0);
    }
    return node_verify(tree, RB_NULL, tree->root, 0, leaf_black_node_count);
}

rb_itor*
rb_itor_new(rb_tree* tree)
{
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
    FREE(itor);
}

bool
rb_itor_valid(const rb_itor* itor)
{
    return itor->node != RB_NULL;
}

void
rb_itor_invalidate(rb_itor* itor)
{
    itor->node = RB_NULL;
}

bool
rb_itor_next(rb_itor* itor)
{
    if (itor->node == RB_NULL)
	rb_itor_first(itor);
    else
	itor->node = node_next(itor->node);
    return itor->node != RB_NULL;
}

bool
rb_itor_prev(rb_itor* itor)
{
    if (itor->node == RB_NULL)
	rb_itor_last(itor);
    else
	itor->node = node_prev(itor->node);
    return itor->node != RB_NULL;
}

bool
rb_itor_nextn(rb_itor* itor, size_t count)
{
    while (count--)
	if (!rb_itor_next(itor))
	    return false;
    return itor->node != RB_NULL;
}

bool
rb_itor_prevn(rb_itor* itor, size_t count)
{
    while (count--)
	if (!rb_itor_prev(itor))
	    return false;
    return itor->node != RB_NULL;
}

bool
rb_itor_first(rb_itor* itor)
{
    if (itor->tree->root == RB_NULL)
	itor->node = RB_NULL;
    else
	itor->node = node_min(itor->tree->root);
    return itor->node != RB_NULL;
}

bool
rb_itor_last(rb_itor* itor)
{
    if (itor->tree->root == RB_NULL)
	itor->node = RB_NULL;
    else
	itor->node = node_max(itor->tree->root);
    return itor->node != RB_NULL;
}

bool
rb_itor_search(rb_itor* itor, const void* key)
{
    rb_node* node = itor->tree->root;
    while (node != RB_NULL) {
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
    itor->node = RB_NULL;
    return false;
}

bool
rb_itor_search_le(rb_itor* itor, const void* key)
{
    return (itor->node = rb_tree_search_le_node(itor->tree, key)) != RB_NULL;
}

bool
rb_itor_search_lt(rb_itor* itor, const void* key)
{
    return (itor->node = rb_tree_search_lt_node(itor->tree, key)) != RB_NULL;
}

bool
rb_itor_search_ge(rb_itor* itor, const void* key)
{
    return (itor->node = rb_tree_search_ge_node(itor->tree, key)) != RB_NULL;
}

bool
rb_itor_search_gt(rb_itor* itor, const void* key)
{
    return (itor->node = rb_tree_search_gt_node(itor->tree, key)) != RB_NULL;
}

const void*
rb_itor_key(const rb_itor* itor)
{
    return itor->node != RB_NULL ? itor->node->key : NULL;
}

void**
rb_itor_datum(rb_itor* itor)
{
    return (itor->node != RB_NULL) ? &itor->node->datum : NULL;
}
