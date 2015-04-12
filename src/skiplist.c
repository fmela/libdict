/*
 * libdict -- skiplist implementation.
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
 * cf. [Pugh 1990], [Sedgewick 1998]
 */

#include "skiplist.h"

#include <string.h>	    /* For memset() */
#include "dict_private.h"

typedef struct skip_node skip_node;

struct skip_node {
    void*		    key;
    void*		    datum;
    skip_node*		    prev;
    unsigned		    link_count;
    skip_node*		    link[0];
};

#define MAX_LINK	    32

struct skiplist {
    skip_node*		    head;
    unsigned		    max_link;
    unsigned		    top_link;
    dict_compare_func	    cmp_func;
    dict_delete_func	    del_func;
    size_t		    count;
};

struct skiplist_itor {
    skiplist*		    list;
    skip_node*		    node;
};

static dict_vtable skiplist_vtable = {
    (dict_inew_func)	    skiplist_dict_itor_new,
    (dict_dfree_func)	    skiplist_free,
    (dict_insert_func)	    skiplist_insert,
    (dict_search_func)	    skiplist_search,
    (dict_search_func)	    NULL,/* search_le: not implemented */
    (dict_search_func)	    NULL,/* search_lt: not implemented */
    (dict_search_func)	    NULL,/* search_ge: not implemented */
    (dict_search_func)	    NULL,/* search_gt: not implemented */
    (dict_remove_func)	    skiplist_remove,
    (dict_clear_func)	    skiplist_clear,
    (dict_traverse_func)    skiplist_traverse,
    (dict_count_func)	    skiplist_count,
    (dict_verify_func)	    skiplist_verify,
    (dict_clone_func)	    skiplist_clone,
};

static itor_vtable skiplist_itor_vtable = {
    (dict_ifree_func)	    skiplist_itor_free,
    (dict_valid_func)	    skiplist_itor_valid,
    (dict_invalidate_func)  skiplist_itor_invalidate,
    (dict_next_func)	    skiplist_itor_next,
    (dict_prev_func)	    skiplist_itor_prev,
    (dict_nextn_func)	    skiplist_itor_nextn,
    (dict_prevn_func)	    skiplist_itor_prevn,
    (dict_first_func)	    skiplist_itor_first,
    (dict_last_func)	    skiplist_itor_last,
    (dict_key_func)	    skiplist_itor_key,
    (dict_data_func)	    skiplist_itor_data,
    (dict_isearch_func)	    skiplist_itor_search,
    (dict_isearch_func)	    NULL,/* itor_search_le: not implemented */
    (dict_isearch_func)	    NULL,/* itor_search_lt: not implemented */
    (dict_isearch_func)	    NULL,/* itor_search_ge: not implemented */
    (dict_isearch_func)	    NULL,/* itor_search_gt: not implemented */
    (dict_iremove_func)	    NULL,/* skiplist_itor_remove not implemented yet */
    (dict_icompare_func)    NULL/* skiplist_itor_compare not implemented yet */
};

static skip_node*   node_new(void* key, unsigned link_count);
static void**	    node_insert(skiplist* list, void* key, skip_node** update);
static unsigned	    rand_link_count(skiplist* list);

skiplist*
skiplist_new(dict_compare_func cmp_func, dict_delete_func del_func,
	     unsigned max_link)
{
    ASSERT(max_link > 0);

    if (max_link > MAX_LINK)
	max_link = MAX_LINK;

    skiplist* list = MALLOC(sizeof(*list));
    if (list) {
	if (!(list->head = node_new(NULL, max_link))) {
	    FREE(list);
	    return NULL;
	}

	list->max_link = max_link;
	list->top_link = 0;
	list->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	list->del_func = del_func;
	list->count = 0;
    }
    return list;
}

dict*
skiplist_dict_new(dict_compare_func cmp_func, dict_delete_func del_func,
		  unsigned max_link) {
    dict* dct = MALLOC(sizeof(*dct));
    if (dct) {
	if (!(dct->_object = skiplist_new(cmp_func, del_func, max_link))) {
	    FREE(dct);
	    return NULL;
	}
	dct->_vtable = &skiplist_vtable;
    }
    return dct;
}

size_t
skiplist_free(skiplist* list)
{
    ASSERT(list != NULL);

    size_t count = skiplist_clear(list);
    FREE(list->head);
    FREE(list);
    return count;
}

skiplist*
skiplist_clone(skiplist* list, dict_key_datum_clone_func clone_func)
{
    ASSERT(list != NULL);

    skiplist* clone = skiplist_new(list->cmp_func, list->del_func,
				   list->max_link);
    if (clone) {
	skip_node* node = list->head->link[0];
	while (node) {
	    bool inserted = false;
	    void** datum = skiplist_insert(clone, node->key, &inserted);
	    if (!datum || !inserted) {
		skiplist_free(clone);
		return NULL;
	    }
	    *datum = node->datum;
	    node = node->link[0];
	}
	if (clone_func) {
	    node = clone->head->link[0];
	    while (node) {
		clone_func(&node->key, &node->datum);
		node = node->link[0];
	    }
	}
    }
    return clone;
}

static void**
node_insert(skiplist* list, void* key, skip_node** update)
{
    const unsigned nlinks = rand_link_count(list);
    ASSERT(nlinks < list->max_link);
    skip_node* x = node_new(key, nlinks);
    if (!x) {
	return NULL;
    }

    if (list->top_link < nlinks) {
	for (unsigned k = list->top_link+1; k <= nlinks; k++) {
	    ASSERT(!update[k]);
	    update[k] = list->head;
	}
	list->top_link = nlinks;
    }

    x->prev = update[0];
    if (update[0]->link[0])
	update[0]->link[0]->prev = x;
    for (unsigned k = 0; k < nlinks; k++) {
	ASSERT(update[k]->link_count > k);
	x->link[k] = update[k]->link[k];
	update[k]->link[k] = x;
    }
    ++list->count;
    return &x->datum;
}

void**
skiplist_insert(skiplist* list, void* key, bool* inserted)
{
    ASSERT(list != NULL);

    skip_node* x = list->head;
    skip_node* update[MAX_LINK] = { 0 };
    for (unsigned k = list->top_link+1; k-->0; ) {
	ASSERT(x->link_count > k);
	while (x->link[k] && list->cmp_func(key, x->link[k]->key) > 0)
	    x = x->link[k];
	update[k] = x;
    }
    x = x->link[0];
    if (x && list->cmp_func(key, x->key) == 0) {
	if (inserted)
	    *inserted = false;
	return &x->datum;
    }
    void** datum = node_insert(list, key, update);
    if (datum) {
	*inserted = true;
    }
    return datum;
}

void*
skiplist_search(skiplist* list, const void* key)
{
    ASSERT(list != NULL);

    skip_node* x = list->head;
    for (unsigned k = list->top_link+1; k-->0;) {
	while (x->link[k]) {
	    int cmp = list->cmp_func(key, x->link[k]->key);
	    if (cmp < 0)
		break;
	    x = x->link[k];
	    if (cmp == 0)
		return x->datum;
	}
    }
    return NULL;
}

bool
skiplist_remove(skiplist* list, const void* key)
{
    ASSERT(list != NULL);

    skip_node* x = list->head;
    skip_node* update[MAX_LINK] = { 0 };
    for (unsigned k = list->top_link+1; k-->0;) {
	ASSERT(x->link_count > k);
	while (x->link[k] && list->cmp_func(key, x->link[k]->key) > 0)
	    x = x->link[k];
	update[k] = x;
    }
    x = x->link[0];
    if (!x || list->cmp_func(key, x->key) != 0)
	return false;
    for (unsigned k = 0; k <= list->top_link; k++) {
	ASSERT(update[k] != NULL);
	ASSERT(update[k]->link_count > k);
	if (update[k]->link[k] != x)
	    break;
	update[k]->link[k] = x->link[k];
    }
    if (x->prev)
	x->prev->link[0] = x->link[0];
    if (x->link[0])
	x->link[0]->prev = x->prev;
    if (list->del_func)
	list->del_func(x->key, x->datum);
    FREE(x);
    while (list->top_link > 0 && !list->head->link[list->top_link-1])
	list->top_link--;
    list->count--;
    return true;
}

size_t
skiplist_clear(skiplist* list)
{
    ASSERT(list != NULL);

    skip_node* node = list->head->link[0];
    while (node) {
	skip_node* next = node->link[0];
	if (list->del_func)
	    list->del_func(node->key, node->datum);
	FREE(node);
	node = next;
    }

    const size_t count = list->count;
    list->count = 0;
    list->head->link[list->top_link] = NULL;
    while (list->top_link)
	list->head->link[--list->top_link] = NULL;

    return count;
}

size_t
skiplist_traverse(skiplist* list, dict_visit_func visit)
{
    ASSERT(list != NULL);
    ASSERT(visit != NULL);

    size_t count = 0;
    for (skip_node* node = list->head->link[0]; node; node = node->link[0]) {
	++count;
	if (!visit(node->key, node->datum))
	    break;
    }
    return count;
}

size_t
skiplist_count(const skiplist* list)
{
    ASSERT(list != NULL);

    return list->count;
}

bool
skiplist_verify(const skiplist* list)
{
    if (list->count == 0) {
	VERIFY(list->top_link == 0);
    } else {
	VERIFY(list->top_link > 0);
    }
    VERIFY(list->top_link < list->max_link);
    for (unsigned i = 0; i < list->top_link; ++i) {
	VERIFY(list->head->link[i] != NULL);
    }
    for (unsigned i = list->top_link; i < list->max_link; ++i) {
	VERIFY(list->head->link[i] == NULL);
    }
    unsigned observed_top_link = 0;

    skip_node* prev = list->head;
    skip_node* node = list->head->link[0];
    VERIFY(prev->prev == NULL);
    while (node) {
	if (observed_top_link < node->link_count)
	    observed_top_link = node->link_count;

	VERIFY(node->prev == prev);
	VERIFY(node->link_count >= 1);
	VERIFY(node->link_count <= list->top_link);
	for (unsigned k = 0; k < node->link_count; k++) {
	    if (node->link[k]) {
		VERIFY(node->link[k]->link_count >= k);
	    }
	}

	prev = node;
	node = node->link[0];
    }
    VERIFY(list->top_link == observed_top_link);
    return true;
}

#define VALID(itor) ((itor)->node && (itor)->node != (itor)->list->head)

skiplist_itor*
skiplist_itor_new(skiplist* list)
{
    ASSERT(list != NULL);

    skiplist_itor* itor = MALLOC(sizeof(*itor));
    if (itor) {
	itor->list = list;
	itor->node = NULL;
    }
    return itor;
}

dict_itor*
skiplist_dict_itor_new(skiplist* list)
{
    ASSERT(list != NULL);

    dict_itor* itor = MALLOC(sizeof(*itor));
    if (itor) {
	if (!(itor->_itor = skiplist_itor_new(list))) {
	    FREE(itor);
	    return NULL;
	}
	itor->_vtable = &skiplist_itor_vtable;
    }
    return itor;
}

void
skiplist_itor_free(skiplist_itor* itor)
{
    ASSERT(itor != NULL);

    FREE(itor);
}

bool
skiplist_itor_valid(const skiplist_itor* itor)
{
    ASSERT(itor != NULL);

    return VALID(itor);
}

void
skiplist_itor_invalidate(skiplist_itor* itor)
{
    ASSERT(itor != NULL);

    itor->node = NULL;
}

bool
skiplist_itor_next(skiplist_itor* itor)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	return skiplist_itor_first(itor);

    itor->node = itor->node->link[0];
    return VALID(itor);
}

bool
skiplist_itor_prev(skiplist_itor* itor)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	return skiplist_itor_last(itor);

    itor->node = itor->node->prev;
    return VALID(itor);
}

bool
skiplist_itor_nextn(skiplist_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!skiplist_itor_next(itor))
	    return false;
    return VALID(itor);
}

bool
skiplist_itor_prevn(skiplist_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!skiplist_itor_prev(itor))
	    return false;
    return VALID(itor);
}

bool
skiplist_itor_first(skiplist_itor* itor)
{
    ASSERT(itor != NULL);

    itor->node = itor->list->head->link[0];
    return VALID(itor);
}

bool
skiplist_itor_last(skiplist_itor* itor)
{
    ASSERT(itor != NULL);

    skip_node* x = itor->list->head;
    for (unsigned k = itor->list->top_link; k-->0;) {
	while (x->link[k])
	    x = x->link[k];
    }
    if (x == itor->list->head) {
	itor->node = NULL;
	return false;
    } else {
	itor->node = x;
	return true;
    }
}

bool
skiplist_itor_search(skiplist_itor* itor, const void* key)
{
    ASSERT(itor != NULL);

    skiplist* list = itor->list;
    skip_node* x = list->head;
    for (unsigned k = list->top_link+1; k-->0;) {
	while (x->link[k]) {
	    int cmp = list->cmp_func(key, x->link[k]->key);
	    if (cmp < 0)
		break;
	    x = x->link[k];
	    if (cmp == 0) {
		itor->node = x;
		return true;
	    }
	}
    }
    itor->node = NULL;
    return false;
}

const void*
skiplist_itor_key(const skiplist_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->node ? itor->node->key : NULL;
}

void**
skiplist_itor_data(skiplist_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->node ? &itor->node->datum : NULL;
}

skip_node*
node_new(void* key, unsigned link_count)
{
    ASSERT(link_count >= 1);

    skip_node* node = MALLOC(sizeof(*node) +
			     sizeof(node->link[0]) * link_count);
    if (node) {
	node->key = key;
	node->datum = NULL;
	node->prev = NULL;
	node->link_count = link_count;
	memset(node->link, 0, sizeof(node->link[0]) * link_count);
    }
    return node;
}

static unsigned
rand_link_count(skiplist* list)
{
    unsigned count = __builtin_ctz(dict_rand()) / 2 + 1;
    return (count >= list->max_link) ? list->max_link - 1 : count;
}
