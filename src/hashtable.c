/*
 * libdict -- chained hash-table, with chains sorted by hash, implementation.
 * cf. [Gonnet 1984], [Knuth 1998]
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

#include "hashtable.h"

#include <string.h> /* For memset() */
#include "dict_private.h"

typedef struct hash_node hash_node;

struct hash_node {
    void*		    key;
    void*		    datum;
    unsigned		    hash;	/* Untruncated hash value. */
    hash_node*		    next;
    /* Only because iterators are bidirectional: */
    hash_node*		    prev;
};

struct hashtable {
    hash_node**		    table;
    unsigned		    size;
    dict_compare_func	    cmp_func;
    dict_hash_func	    hash_func;
    dict_delete_func	    del_func;
    size_t		    count;
};

struct hashtable_itor {
    hashtable*		    table;
    hash_node*		    node;
    unsigned		    slot;
};

static dict_vtable hashtable_vtable = {
    (dict_inew_func)	    hashtable_dict_itor_new,
    (dict_dfree_func)	    hashtable_free,
    (dict_insert_func)	    hashtable_insert,
    (dict_search_func)	    hashtable_search,
    (dict_remove_func)	    hashtable_remove,
    (dict_clear_func)	    hashtable_clear,
    (dict_traverse_func)    hashtable_traverse,
    (dict_count_func)	    hashtable_count
};

static itor_vtable hashtable_itor_vtable = {
    (dict_ifree_func)	    hashtable_itor_free,
    (dict_valid_func)	    hashtable_itor_valid,
    (dict_invalidate_func)  hashtable_itor_invalidate,
    (dict_next_func)	    hashtable_itor_next,
    (dict_prev_func)	    hashtable_itor_prev,
    (dict_nextn_func)	    hashtable_itor_nextn,
    (dict_prevn_func)	    hashtable_itor_prevn,
    (dict_first_func)	    hashtable_itor_first,
    (dict_last_func)	    hashtable_itor_last,
    (dict_key_func)	    hashtable_itor_key,
    (dict_data_func)	    hashtable_itor_data,
    (dict_set_data_func)    hashtable_itor_set_data,
    (dict_iremove_func)	    NULL,/* hashtable_itor_remove not implemented yet */
    (dict_icompare_func)    NULL/* hashtable_itor_compare not implemented yet */
};

hashtable *
hashtable_new(dict_compare_func cmp_func, dict_hash_func hash_func,
	      dict_delete_func del_func, unsigned size)
{
    ASSERT(hash_func != NULL);
    ASSERT(size != 0);

    hashtable *table = MALLOC(sizeof(*table));
    if (table) {
	table->table = MALLOC(size * sizeof(hash_node *));
	if (!table->table) {
	    FREE(table);
	    return NULL;
	}
	memset(table->table, 0, size * sizeof(hash_node *));
	table->size = size;
	table->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	table->hash_func = hash_func;
	table->del_func = del_func;
	table->count = 0;
    }
    return table;
}

dict *
hashtable_dict_new(dict_compare_func cmp_func, dict_hash_func hash_func,
		   dict_delete_func del_func, unsigned size)
{
    ASSERT(hash_func != NULL);
    ASSERT(size != 0);

    dict *dct = MALLOC(sizeof(*dct));
    if (dct) {
	dct->_object = hashtable_new(cmp_func, hash_func, del_func, size);
	if (!dct->_object) {
	    FREE(dct);
	    return NULL;
	}
	dct->_vtable = &hashtable_vtable;
    }
    return dct;
}

size_t
hashtable_free(hashtable *table)
{
    ASSERT(table != NULL);

    size_t count = hashtable_clear(table);
    FREE(table->table);
    FREE(table);
    return count;
}

bool
hashtable_insert(hashtable *table, void *key, void ***datum_location)
{
    ASSERT(table != NULL);

    const unsigned hash = table->hash_func(key);
    const unsigned mhash = hash % table->size;
    hash_node *node = table->table[mhash], *prev = NULL;
    while (node && hash >= node->hash) {
	if (hash == node->hash && table->cmp_func(key, node->key) == 0) {
	    if (datum_location)
		*datum_location = &node->datum;
	    return false;
	}
	prev = node;
	node = node->next;
    }

    hash_node *add = MALLOC(sizeof(*add));
    if (!add) {
	if (datum_location)
	    *datum_location = NULL;
	return false;
    }

    add->key = key;
    add->datum = NULL;
    if (datum_location)
	*datum_location = &add->datum;
    add->hash = hash;
    add->prev = prev;
    if (prev)
	prev->next = add;
    else
	table->table[mhash] = add;
    add->next = node;
    if (node)
	node->prev = add;

    table->count++;
    return true;
}

void *
hashtable_search(hashtable *table, const void *key)
{
    ASSERT(table != NULL);

    const unsigned hash = table->hash_func(key);
    hash_node *node = table->table[hash % table->size];
    while (node && hash >= node->hash) {
	if (hash == node->hash && table->cmp_func(key, node->key) == 0)
	    return node->datum;
	node = node->next;
    }
    return NULL;
}

bool
hashtable_remove(hashtable *table, const void *key)
{
    ASSERT(table != NULL);

    const unsigned hash = table->hash_func(key);
    const unsigned mhash = hash % table->size;

    hash_node *node = table->table[mhash], *prev = NULL;
    while (node && hash >= node->hash) {
	if (hash == node->hash && table->cmp_func(key, node->key) == 0) {
	    if (prev)
		prev->next = node->next;
	    else
		table->table[mhash] = node->next;

	    if (node->next)
		node->next->prev = prev;

	    if (table->del_func)
		table->del_func(node->key, node->datum);

	    FREE(node);
	    table->count--;
	    return true;
	}
	prev = node;
	node = node->next;
    }
    return false;
}

size_t
hashtable_clear(hashtable *table)
{
    ASSERT(table != NULL);

    for (unsigned slot = 0; slot < table->size; slot++) {
	hash_node *node = table->table[slot];
	while (node != NULL) {
	    hash_node *next = node->next;
	    if (table->del_func)
		table->del_func(node->key, node->datum);
	    FREE(node);
	    node = next;
	}
	table->table[slot] = NULL;
    }

    const size_t count = table->count;
    table->count = 0;
    return count;
}

size_t
hashtable_traverse(hashtable *table, dict_visit_func visit)
{
    ASSERT(table != NULL);
    ASSERT(visit != NULL);

    size_t count = 0;
    for (unsigned i = 0; i < table->size; i++)
	for (hash_node *node = table->table[i]; node; node = node->next) {
	    ++count;
	    if (!visit(node->key, node->datum))
		return count;
	}
    return count;
}

size_t
hashtable_count(const hashtable *table)
{
    ASSERT(table != NULL);

    return table->count;
}

size_t
hashtable_size(const hashtable *table)
{
    ASSERT(table != NULL);

    return table->size;
}

size_t
hashtable_slots_used(const hashtable *table)
{
    ASSERT(table != NULL);

    size_t count = 0;
    for (unsigned i = 0; i < table->size; i++)
	if (table->table[i])
	    count++;
    return count;
}

bool
hashtable_resize(hashtable *table, unsigned size)
{
    ASSERT(table != NULL);
    ASSERT(size != 0);

    if (table->size == size)
	return 0;

    /* TODO: use REALLOC instead of MALLOC. */
    const size_t table_memory = size * sizeof(hash_node *);
    hash_node **ntable = MALLOC(table_memory);
    if (!ntable)
	return false;
    memset(ntable, 0, table_memory);

    /* FIXME: insert data in sorted order in the new list. */
    for (unsigned i = 0; i < table->size; i++) {
	hash_node *node = table->table[i];
	while (node) {
	    hash_node *next = node->next;
	    unsigned hash = table->hash_func(node->key) % size;
	    node->next = ntable[hash];
	    node->prev = NULL;
	    if (ntable[hash])
		ntable[hash]->prev = node;
	    ntable[hash] = node;
	    node = next;
	}
    }

    FREE(table->table);
    table->table = ntable;
    table->size = size;

    return 0;
}

hashtable_itor *
hashtable_itor_new(hashtable *table)
{
    ASSERT(table != NULL);

    hashtable_itor *itor = MALLOC(sizeof(*itor));
    if (itor) {
	itor->table = table;
	itor->node = NULL;
	itor->slot = 0;
    }
    return itor;
}

dict_itor *
hashtable_dict_itor_new(hashtable *table)
{
    ASSERT(table != NULL);

    dict_itor *itor = MALLOC(sizeof(*itor));
    if (itor) {
	if (!(itor->_itor = hashtable_itor_new(table))) {
	    FREE(itor);
	    return NULL;
	}
	itor->_vtable = &hashtable_itor_vtable;
    }
    return itor;
}

void
hashtable_itor_free(hashtable_itor *itor)
{
    ASSERT(itor != NULL);

    FREE(itor);
}

bool
hashtable_itor_valid(const hashtable_itor *itor)
{
    ASSERT(itor != NULL);

    return itor->node != NULL;
}

void
hashtable_itor_invalidate(hashtable_itor *itor)
{
    ASSERT(itor != NULL);

    itor->node = NULL;
    itor->slot = 0;
}

bool
hashtable_itor_next(hashtable_itor *itor)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	return hashtable_itor_first(itor);

    itor->node = itor->node->next;
    if (itor->node)
	return true;

    unsigned slot = itor->slot;
    while (++slot < itor->table->size) {
	if (itor->table->table[slot]) {
	    itor->node = itor->table->table[slot];
	    itor->slot = slot;
	    return true;
	}
    }
    itor->node = NULL;
    itor->slot = 0;
    return itor->node != NULL;
}

bool
hashtable_itor_prev(hashtable_itor *itor)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	return hashtable_itor_last(itor);

    itor->node = itor->node->prev;
    if (itor->node)
	return true;

    unsigned slot = itor->slot;
    while (slot > 0) {
	hash_node *node = itor->table->table[--slot];
	if (node) {
	    while (node->next)
		node = node->next;
	    itor->node = node;
	    itor->slot = slot;
	    return true;
	}
    }
    itor->node = NULL;
    itor->slot = 0;
    return false;
}

bool
hashtable_itor_nextn(hashtable_itor *itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!hashtable_itor_next(itor))
	    return false;
    return itor->node != NULL;
}

bool
hashtable_itor_prevn(hashtable_itor *itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!hashtable_itor_prev(itor))
	    return false;
    return itor->node != NULL;
}

bool
hashtable_itor_first(hashtable_itor *itor)
{
    ASSERT(itor != NULL);

    for (unsigned slot = 0; slot < itor->table->size; ++slot)
	if (itor->table->table[slot]) {
	    itor->node = itor->table->table[slot];
	    itor->slot = slot;
	    return true;
	}
    itor->node = NULL;
    itor->slot = 0;
    return false;
}

bool
hashtable_itor_last(hashtable_itor *itor)
{
    ASSERT(itor != NULL);

    for (unsigned slot = itor->table->size; slot > 0;)
	if (itor->table->table[--slot]) {
	    hash_node *node;
	    for (node = itor->table->table[slot]; node->next; node = node->next)
		/* void */;
	    itor->node = node;
	    itor->slot = slot;
	    return true;
	}
    itor->node = NULL;
    itor->slot = 0;
    return false;
}

bool
hashtable_itor_search(hashtable_itor *itor, const void *key)
{
    const unsigned hash = itor->table->hash_func(key);
    const unsigned mhash = hash % itor->table->size;
    hash_node *node = itor->table->table[mhash];
    while (node && hash >= node->hash) {
	if (hash == node->hash && itor->table->cmp_func(key, node->key) == 0) {
	    itor->node = node;
	    itor->slot = mhash;
	    return true;
	}
    }
    itor->node = NULL;
    itor->slot = 0;
    return false;
}

const void *
hashtable_itor_key(const hashtable_itor *itor)
{
    ASSERT(itor != NULL);

    return itor->node ? itor->node->key : NULL;
}

void *
hashtable_itor_data(hashtable_itor *itor)
{
    ASSERT(itor != NULL);

    return itor->node ? itor->node->datum : NULL;
}

bool
hashtable_itor_set_data(hashtable_itor *itor, void *datum, void **prev_datum)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	return false;

    if (prev_datum)
	*prev_datum = itor->node->datum;
    itor->node->datum = datum;
    return true;
}
