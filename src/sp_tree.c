/*
 * libdict -- splay tree implementation.
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
 * cf. [Sleator and Tarjan, 1985], [Tarjan 1985], [Tarjan 1983]
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

#include "sp_tree.h"

#include "dict_private.h"
#include "tree_common.h"

typedef struct sp_node sp_node;
struct sp_node {
    TREE_NODE_FIELDS(sp_node);
};

struct sp_tree {
    TREE_FIELDS(sp_node);
};

struct sp_itor {
    TREE_ITERATOR_FIELDS(sp_tree, sp_node);
};

static dict_vtable sp_tree_vtable = {
    (dict_inew_func)	    sp_dict_itor_new,
    (dict_dfree_func)	    tree_free,
    (dict_insert_func)	    sp_tree_insert,
    (dict_search_func)	    sp_tree_search,
    (dict_search_func)	    tree_search_le,
    (dict_search_func)	    tree_search_lt,
    (dict_search_func)	    tree_search_ge,
    (dict_search_func)	    tree_search_gt,
    (dict_remove_func)	    sp_tree_remove,
    (dict_clear_func)	    tree_clear,
    (dict_traverse_func)    tree_traverse,
    (dict_count_func)	    tree_count,
    (dict_verify_func)	    sp_tree_verify,
    (dict_clone_func)	    sp_tree_clone,
};

static itor_vtable sp_tree_itor_vtable = {
    (dict_ifree_func)	    tree_iterator_free,
    (dict_valid_func)	    tree_iterator_valid,
    (dict_invalidate_func)  tree_iterator_invalidate,
    (dict_next_func)	    tree_iterator_next,
    (dict_prev_func)	    tree_iterator_prev,
    (dict_nextn_func)	    tree_iterator_next_n,
    (dict_prevn_func)	    tree_iterator_prev_n,
    (dict_first_func)	    tree_iterator_first,
    (dict_last_func)	    tree_iterator_last,
    (dict_key_func)	    tree_iterator_key,
    (dict_data_func)	    tree_iterator_data,
    (dict_isearch_func)	    sp_itor_search,
    (dict_isearch_func)	    tree_iterator_search_le,
    (dict_isearch_func)	    tree_iterator_search_lt,
    (dict_isearch_func)	    tree_iterator_search_ge,
    (dict_isearch_func)	    tree_iterator_search_gt,
    (dict_iremove_func)	    NULL,/* sp_itor_remove not implemented yet */
    (dict_icompare_func)    NULL /* sp_itor_compare not implemented yet */
};

static sp_node*	node_new(void* key);
static size_t	node_height(const sp_node* node);
static size_t	node_mheight(const sp_node* node);
static size_t	node_pathlen(const sp_node* node, size_t level);
static void	splay(sp_tree* t, sp_node* n);

sp_tree*
sp_tree_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
    sp_tree* tree = MALLOC(sizeof(*tree));
    if (tree) {
	tree->root = NULL;
	tree->count = 0;
	tree->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	tree->del_func = del_func;
	tree->rotation_count = 0;
    }
    return tree;
}

dict*
sp_dict_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
    dict* dct = MALLOC(sizeof(*dct));
    if (dct) {
	if (!(dct->_object = sp_tree_new(cmp_func, del_func))) {
	    FREE(dct);
	    return NULL;
	}
	dct->_vtable = &sp_tree_vtable;
    }
    return dct;
}

size_t
sp_tree_free(sp_tree* tree)
{
    ASSERT(tree != NULL);

    size_t count = sp_tree_clear(tree);
    FREE(tree);
    return count;
}

sp_tree*
sp_tree_clone(sp_tree* tree, dict_key_datum_clone_func clone_func)
{
    ASSERT(tree != NULL);

    return tree_clone(tree, sizeof(sp_tree), sizeof(sp_node), clone_func);
}

size_t
sp_tree_clear(sp_tree* tree)
{
    ASSERT(tree != NULL);

    const size_t count = tree->count;
    sp_node* node = tree->root;
    while (node) {
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
	--tree->count;

	sp_node* parent = node->parent;
	if (parent) {
	    if (parent->llink == node)
	parent->llink = NULL;
	    else
	parent->rlink = NULL;
	}
	FREE(node);
	node = parent;
    }

    tree->root = NULL;
    ASSERT(tree->count == 0);
    return count;
}

static void
splay(sp_tree* t, sp_node* n)
{
    unsigned rotations = 0;
    for (;;) {
	sp_node* p = n->parent;
	if (!p)
	    break;

	sp_node* pp = p->parent;
	if (!pp) {
	    /* Parent is the root; simply rotate root left or right so that |n|
	     * becomes new root. */
	    if (p->llink == n) {
		if ((p->llink = n->rlink) != NULL)
		    p->llink->parent = p;
		n->rlink = p;
		++rotations;
	    } else {
		if ((p->rlink = n->llink) != NULL)
		    p->rlink->parent = p;
		n->llink = p;
	    }
	    p->parent = n;
	    t->root = n;
	    n->parent = NULL;
	    rotations += 1;
	    break;
	}

	rotations += 2;
	sp_node* ppp = pp->parent;
	if (p->llink == n) {
	    if (pp->llink == p) {
		/* Rotate parent right, then node right. */
		sp_node* pr = p->rlink;
		p->rlink = pp;
		pp->parent = p;
		if ((pp->llink = pr) != NULL)
		    pr->parent = pp;

		sp_node* nr = n->rlink;
		n->rlink = p;
		p->parent = n;
		if ((p->llink = nr) != NULL)
		    nr->parent = p;
	    } else {
		/* Rotate node right, then parent left. */
		sp_node* nr = n->rlink;
		n->rlink = p;
		p->parent = n;
		if ((p->llink = nr) != NULL)
		    nr->parent = p;

		sp_node* nl = n->llink;
		n->llink = pp;
		pp->parent = n;
		if ((pp->rlink = nl) != NULL)
		    nl->parent = pp;
	    }
	} else {
	    if (pp->rlink == p) {
		/* Rotate parent left, then node left. */
		sp_node* pl = p->llink;
		p->llink = pp;
		pp->parent = p;
		if ((pp->rlink = pl) != NULL)
		    pl->parent = pp;

		sp_node* nl = n->llink;
		n->llink = p;
		p->parent = n;
		if ((p->rlink = nl) != NULL)
		    nl->parent = p;
	    } else {
		/* Rotate node left, then parent right. */
		sp_node* nl = n->llink;
		n->llink = p;
		p->parent = n;
		if ((p->rlink = nl) != NULL)
		    nl->parent = p;

		sp_node* nr = n->rlink;
		n->rlink = pp;
		pp->parent = n;
		if ((pp->llink = nr) != NULL)
		    nr->parent = pp;
	    }
	}
	n->parent = ppp;
	if (ppp) {
	    if (ppp->llink == pp)
		ppp->llink = n;
	    else
		ppp->rlink = n;
	} else {
	    t->root = n;
	    break;
	}
    }
    t->rotation_count += rotations;
}

void**
sp_tree_insert(sp_tree* tree, void* key, bool* inserted)
{
    ASSERT(tree != NULL);

    int cmp = 0;
    sp_node* node = tree->root;
    sp_node* parent = NULL;
    while (node) {
	cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    parent = node, node = node->llink;
	else if (cmp)
	    parent = node, node = node->rlink;
	else {
	    if (inserted)
		*inserted = false;
	    return &node->datum;
	}
    }

    if (!(node = node_new(key)))
	return NULL;
    if (inserted)
	*inserted = true;
    if (!(node->parent = parent)) {
	ASSERT(tree->count == 0);
	ASSERT(tree->root == NULL);
	tree->root = node;
	tree->count = 1;
    } else {
	if (cmp < 0)
	    parent->llink = node;
	else
	    parent->rlink = node;
	splay(tree, node);
	++tree->count;
    }
    ASSERT(tree->root == node);
    return &node->datum;
}

void*
sp_tree_search(sp_tree* tree, const void* key)
{
    ASSERT(tree != NULL);

    sp_node* node = tree->root;
    sp_node* parent = NULL;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    parent = node, node = node->llink;
	else if (cmp)
	    parent = node, node = node->rlink;
	else {
	    splay(tree, node);
	    ASSERT(tree->root == node);
	    return node->datum;
	}
    }
    if (parent) {
	/* XXX Splay last node seen until it becomes the new root. */
	splay(tree, parent);
	ASSERT(tree->root == parent);
    }
    return NULL;
}

bool
sp_tree_remove(sp_tree* tree, const void* key)
{
    ASSERT(tree != NULL);

    sp_node* node = tree->root;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp)
	    node = node->rlink;
	else
	    break;
    }
    if (!node)
	return false;

    sp_node* out;
    if (!node->llink || !node->rlink) {
	out = node;
    } else {
	void* tmp;
	/* This is sure to screw up iterators that were positioned at the node
	 * "out". */
	for (out = node->rlink; out->llink; out = out->llink)
	    /* void */;
	SWAP(node->key, out->key, tmp);
	SWAP(node->datum, out->datum, tmp);
    }

    sp_node* temp = out->llink ? out->llink : out->rlink;
    sp_node* parent = out->parent;
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
    if (temp) {
	splay(tree, temp);
	ASSERT(tree->root == temp);
    }

    FREE(out);
    --tree->count;
    return true;
}

size_t
sp_tree_traverse(sp_tree* tree, dict_visit_func visit)
{
    ASSERT(tree != NULL);
    ASSERT(visit != NULL);

    size_t count = 0;
    if (tree->root) {
	sp_node* node = tree_node_min(tree->root);
	do {
	    ++count;
	    if (!visit(node->key, node->datum))
		break;
	    node = tree_node_next(node);
	} while (node);
    }
    return count;
}

size_t
sp_tree_count(const sp_tree* tree)
{
    ASSERT(tree != NULL);

    return tree_count(tree);
}

size_t
sp_tree_height(const sp_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_height(tree->root) : 0;
}

size_t
sp_tree_mheight(const sp_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_mheight(tree->root) : 0;
}

size_t
sp_tree_pathlen(const sp_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_pathlen(tree->root, 1) : 0;
}

const void*
sp_tree_min(const sp_tree* tree)
{
    ASSERT(tree != NULL);

    if (!tree->root)
	return NULL;

    const sp_node* node = tree->root;
    for (; node->llink; node = node->llink)
	/* void */;
    return node->key;
}

const void*
sp_tree_max(const sp_tree* tree)
{
    ASSERT(tree != NULL);

    if (!tree->root)
	return NULL;

    const sp_node* node = tree->root;
    for (; node->rlink; node = node->rlink)
	/* void */;
    return node->key;
}

static sp_node*
node_new(void* key)
{
    sp_node* node = MALLOC(sizeof(*node));
    if (node) {
	node->key = key;
	node->datum = NULL;
	node->parent = NULL;
	node->llink = NULL;
	node->rlink = NULL;
    }
    return node;
}

static size_t
node_height(const sp_node* node)
{
    size_t l = node->llink ? node_height(node->llink) + 1 : 0;
    size_t r = node->rlink ? node_height(node->rlink) + 1 : 0;
    return MAX(l, r);
}

static size_t
node_mheight(const sp_node* node)
{
    size_t l = node->llink ? node_mheight(node->llink) + 1 : 0;
    size_t r = node->rlink ? node_mheight(node->rlink) + 1 : 0;
    return MIN(l, r);
}

static size_t
node_pathlen(const sp_node* node, size_t level)
{
    size_t n = 0;

    ASSERT(node != NULL);

    if (node->llink)
	n += level + node_pathlen(node->llink, level + 1);
    if (node->rlink)
	n += level + node_pathlen(node->rlink, level + 1);
    return n;
}

static bool
node_verify(const sp_tree* tree, const sp_node* parent, const sp_node* node)
{
    ASSERT(tree != NULL);

    if (!parent) {
	VERIFY(tree->root == node);
    } else {
	VERIFY(parent->llink == node || parent->rlink == node);
    }
    if (node) {
	VERIFY(node->parent == parent);
	if (!node_verify(tree, node, node->llink) ||
	    !node_verify(tree, node, node->rlink))
	    return false;
    }
    return true;
}

bool
sp_tree_verify(const sp_tree* tree)
{
    ASSERT(tree != NULL);

    if (tree->root) {
	VERIFY(tree->count > 0);
    } else {
	VERIFY(tree->count == 0);
    }
    return node_verify(tree, NULL, tree->root);
}

sp_itor*
sp_itor_new(sp_tree* tree)
{
    ASSERT(tree != NULL);

    sp_itor* itor = MALLOC(sizeof(*itor));
    if (itor) {
	itor->tree = tree;
	itor->node = NULL;
    }
    return itor;
}

dict_itor*
sp_dict_itor_new(sp_tree* tree)
{
    ASSERT(tree != NULL);

    dict_itor* itor = MALLOC(sizeof(*itor));
    if (itor) {
	if (!(itor->_itor = sp_itor_new(tree))) {
	    FREE(itor);
	    return NULL;
	}
	itor->_vtable = &sp_tree_itor_vtable;
    }
    return itor;
}

void
sp_itor_free(sp_itor* itor)
{
    ASSERT(itor != NULL);

    tree_iterator_free(itor);
}

bool
sp_itor_valid(const sp_itor* itor)
{
    ASSERT(itor != NULL);

    return tree_iterator_valid(itor);
}

void
sp_itor_invalidate(sp_itor* itor)
{
    ASSERT(itor != NULL);

    tree_iterator_invalidate(itor);
}

bool
sp_itor_next(sp_itor* itor)
{
    ASSERT(itor != NULL);

    return tree_iterator_next(itor);
}

bool
sp_itor_prev(sp_itor* itor)
{
    ASSERT(itor != NULL);

    return tree_iterator_prev(itor);
}

bool
sp_itor_nextn(sp_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    return tree_iterator_next_n(itor, count);
}

bool
sp_itor_prevn(sp_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    return tree_iterator_prev_n(itor, count);
}

bool
sp_itor_first(sp_itor* itor)
{
    ASSERT(itor != NULL);

    return tree_iterator_first(itor);
}

bool
sp_itor_last(sp_itor* itor)
{
    ASSERT(itor != NULL);

    return tree_iterator_last(itor);
}

bool
sp_itor_search(sp_itor* itor, const void* key)
{
    ASSERT(itor != NULL);

    /* TODO: use algorithm from sp_tree_search() */
    return tree_iterator_search(itor, key);
}

const void*
sp_itor_key(const sp_itor* itor)
{
    ASSERT(itor != NULL);

    return tree_iterator_key(itor);
}

void**
sp_itor_data(sp_itor* itor)
{
    ASSERT(itor != NULL);

    return tree_iterator_data(itor);
}
