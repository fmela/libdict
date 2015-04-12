/*
 * libdict -- chained hash-table, with chains sorted by hash, implementation.
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
 * cf. [Gonnet 1984], [Knuth 1998]
 */

#include "hashtable2.h"

#include <stdbool.h>
#include <string.h> /* For memset() */
#include "dict_private.h"
#include "hashtable_common.h"

/* TODO: make this configurable in the constructor methods */
#define LOADFACTOR_NUMERATOR	2
#define LOADFACTOR_DENOMINATOR	3
#if LOADFACTOR_NUMERATOR > LOADFACTOR_DENOMINATOR
# error LOADFACTOR_NUMERATOR must be less than LOADFACTOR_DENOMINATOR
#endif

typedef struct hash_node hash_node;

struct hash_node {
    void*		    key;
    void*		    datum;
    unsigned		    hash;	/* Untruncated hash value; 0 iff unoccupied. */
};

struct hashtable2 {
    unsigned		    size;
    size_t		    count;
    dict_compare_func	    cmp_func;
    dict_hash_func	    hash_func;
    dict_delete_func	    del_func;
    hash_node*		    table;
};

struct hashtable2_itor {
    hashtable2*		    table;
    int			    slot;
};

static dict_vtable hashtable2_vtable = {
    (dict_inew_func)	    hashtable2_dict_itor_new,
    (dict_dfree_func)	    hashtable2_free,
    (dict_insert_func)	    hashtable2_insert,
    (dict_search_func)	    hashtable2_search,
    (dict_search_func)	    NULL,/* search_le: not implemented */
    (dict_search_func)	    NULL,/* search_lt: not implemented */
    (dict_search_func)	    NULL,/* search_ge: not implemented */
    (dict_search_func)	    NULL,/* search_gt: not implemented */
    (dict_remove_func)	    hashtable2_remove,
    (dict_clear_func)	    hashtable2_clear,
    (dict_traverse_func)    hashtable2_traverse,
    (dict_count_func)	    hashtable2_count,
    (dict_verify_func)	    hashtable2_verify,
    (dict_clone_func)	    hashtable2_clone,
};

static itor_vtable hashtable2_itor_vtable = {
    (dict_ifree_func)	    hashtable2_itor_free,
    (dict_valid_func)	    hashtable2_itor_valid,
    (dict_invalidate_func)  hashtable2_itor_invalidate,
    (dict_next_func)	    hashtable2_itor_next,
    (dict_prev_func)	    hashtable2_itor_prev,
    (dict_nextn_func)	    hashtable2_itor_nextn,
    (dict_prevn_func)	    hashtable2_itor_prevn,
    (dict_first_func)	    hashtable2_itor_first,
    (dict_last_func)	    hashtable2_itor_last,
    (dict_key_func)	    hashtable2_itor_key,
    (dict_data_func)	    hashtable2_itor_data,
    (dict_isearch_func)	    hashtable2_itor_search,
    (dict_isearch_func)	    NULL,/* itor_search_le: not implemented */
    (dict_isearch_func)	    NULL,/* itor_search_lt: not implemented */
    (dict_isearch_func)	    NULL,/* itor_search_ge: not implemented */
    (dict_isearch_func)	    NULL,/* itor_search_gt: not implemented */
    (dict_iremove_func)	    NULL,/* hashtable2_itor_remove not implemented yet */
    (dict_icompare_func)    NULL,/* hashtable2_itor_compare not implemented yet */
};

hashtable2*
hashtable2_new(dict_compare_func cmp_func, dict_hash_func hash_func,
	       dict_delete_func del_func, unsigned initial_size)
{
    ASSERT(hash_func != NULL);
    ASSERT(initial_size != 0);

    hashtable2* table = MALLOC(sizeof(*table));
    if (table) {
	table->size = dict_prime_geq(initial_size);
	table->table = MALLOC(table->size * sizeof(hash_node));
	if (!table->table) {
	    FREE(table);
	    return NULL;
	}
	memset(table->table, 0, table->size * sizeof(hash_node));
	table->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	table->hash_func = hash_func;
	table->del_func = del_func;
	table->count = 0;
    }
    return table;
}

hashtable2*
hashtable2_clone(hashtable2* table, dict_key_datum_clone_func clone_func)
{
    ASSERT(table != NULL);

    hashtable2* clone = hashtable2_new(table->cmp_func, table->hash_func,
				       table->del_func, table->size);
    if (!clone) {
	return NULL;
    }
    memcpy(clone->table, table->table, sizeof(hash_node) * table->size);
    clone->count = table->count;
    if (clone_func) {
	for (hash_node *node = clone->table, *end = clone->table + table->size; node != end; ++node) {
	    if (node->hash) {
		clone_func(&node->key, &node->datum);
	    }
	}
    }
    return clone;
}

dict*
hashtable2_dict_new(dict_compare_func cmp_func, dict_hash_func hash_func,
		    dict_delete_func del_func, unsigned size)
{
    ASSERT(hash_func != NULL);
    ASSERT(size != 0);

    dict* dct = MALLOC(sizeof(*dct));
    if (dct) {
	dct->_object = hashtable2_new(cmp_func, hash_func, del_func, size);
	if (!dct->_object) {
	    FREE(dct);
	    return NULL;
	}
	dct->_vtable = &hashtable2_vtable;
    }
    return dct;
}

size_t
hashtable2_free(hashtable2* table)
{
    ASSERT(table != NULL);

    size_t count = hashtable2_clear(table);
    FREE(table->table);
    FREE(table);
    return count;
}

static void**
insert(hashtable2* table, void *key, unsigned hash, bool* inserted)
{
    ASSERT(hash != 0);

    const unsigned truncated_hash = hash % table->size;
    unsigned index = truncated_hash;
    do {
	hash_node *node = &table->table[index];
	if (!node->hash) {
	    node->key = key;
	    node->datum = NULL;
	    node->hash = hash;
	    table->count++;
	    if (inserted)
		*inserted = true;
	    return &node->datum;
	}
	if (node->hash == hash && table->cmp_func(key, node->key) == 0) {
	    if (inserted)
		*inserted = false;
	    return &node->datum;
	}
	if (++index == table->size) {
	    index = 0;
	}
    } while (index != truncated_hash);
    /* No room for new element! */
    if (inserted)
	*inserted = false;
    return NULL;
}

void**
hashtable2_insert(hashtable2* table, void* key, bool* inserted)
{
    ASSERT(table != NULL);

    if (LOADFACTOR_DENOMINATOR * table->count >= LOADFACTOR_NUMERATOR * table->size) {
	/* Load factor too high: resize the table up to the next prime. */
	if (!hashtable2_resize(table, table->size + 1)) {
	    /* Out of memory, or other error? */
	    if (inserted)
		*inserted = false;
	    return NULL;
	}
    }
    const unsigned raw_hash = table->hash_func(key);
    const unsigned hash = (raw_hash == 0) ? ~(unsigned)0 : raw_hash;
    void **datum_location = insert(table, key, hash, inserted);
    ASSERT(datum_location != NULL);
    return datum_location;
}

void*
hashtable2_search(hashtable2* table, const void* key)
{
    ASSERT(table != NULL);

    const unsigned raw_hash = table->hash_func(key);
    const unsigned hash = (raw_hash == 0) ? ~(unsigned)0 : raw_hash;
    const unsigned truncated_hash = hash % table->size;
    unsigned index = truncated_hash;
    do {
	hash_node *node = &table->table[index];
	if (!node->hash) { /* Not occupied. */
	    return NULL;
	}
	if (node->hash == hash && table->cmp_func(key, node->key) == 0) {
	    return node->datum;
	}
	if (++index == table->size) {
	    index = 0;
	}
    } while (index != truncated_hash);
    return NULL;
}

#if 0
static int
index_of_node_to_shift(hashtable2* table, unsigned truncated_hash, unsigned index)
{
    int last_index = -1;
    do {
	hash_node* node = &table->table[index];
	if (!node->hash) {
	    break;
	}
	if (node->hash % table->size == truncated_hash) {
	    last_index = index;
	}
	if (++index == table->size) {
	    index = 0;
	}
    } while (index != truncated_hash);
    return last_index;
}
#endif

static void
remove_cleanup(hashtable2* table, unsigned truncated_hash, unsigned index)
{
    do {
	hash_node* node = &table->table[index];
	const unsigned hash = node->hash;
	if (!hash) { /* Not occupied. */
	    break;
	}
	void *datum = node->datum;
	node->hash = 0;
	table->count--;

	bool inserted = false;
	void **datum_location = insert(table, node->key, hash, &inserted);
	ASSERT(inserted);
	ASSERT(datum_location != NULL);
	*datum_location = datum;

	if (++index == table->size) {
	    index = 0;
	}
    } while (index != truncated_hash);
}

bool
hashtable2_remove(hashtable2* table, const void* key)
{
    ASSERT(table != NULL);

    const unsigned raw_hash = table->hash_func(key);
    const unsigned hash = (raw_hash == 0) ? ~(unsigned)0 : raw_hash;
    const unsigned truncated_hash = hash % table->size;
    unsigned index = truncated_hash;
    do {
	hash_node *node = &table->table[index];
	if (!node->hash) { /* Not occupied. */
	    return NULL;
	}
	if (node->hash == hash && table->cmp_func(key, node->key) == 0) {
	    if (table->del_func)
		table->del_func(node->key, node->datum);
	    node->hash = 0;
	    table->count--;
	    remove_cleanup(table, truncated_hash, (index + 1) % table->size);
	    return true;
	}
	if (++index == table->size) {
	    index = 0;
	}
    } while (index != truncated_hash);
    return false;
}

size_t
hashtable2_clear(hashtable2* table)
{
    ASSERT(table != NULL);

    hash_node *node = table->table;
    hash_node *end = table->table + table->size;
    for (; node != end; ++node) {
	if (node->hash) {
	    if (table->del_func)
		table->del_func(node->key, node->datum);
	    node->hash = 0;
	}
    }

    const size_t count = table->count;
    table->count = 0;
    return count;
}

size_t
hashtable2_traverse(hashtable2* table, dict_visit_func visit)
{
    ASSERT(table != NULL);
    ASSERT(visit != NULL);

    size_t count = 0;
    hash_node *node = table->table;
    hash_node *end = table->table + table->size;
    for (; node != end; ++node) {
	if (node->hash) {
	    ++count;
	    if (!visit(node->key, node->datum))
		break;
	}
    }
    return count;
}

size_t
hashtable2_count(const hashtable2* table)
{
    ASSERT(table != NULL);

    return table->count;
}

size_t
hashtable2_size(const hashtable2* table)
{
    ASSERT(table != NULL);

    return table->size;
}

size_t
hashtable2_slots_used(const hashtable2* table)
{
    ASSERT(table != NULL);

    return table->count;
}

bool
hashtable2_resize(hashtable2* table, unsigned new_size)
{
    ASSERT(table != NULL);
    ASSERT(new_size != 0);

    new_size = dict_prime_geq(new_size);
    if (table->size == new_size)
	return true;

    if (table->count > new_size) {
	/* The number of records already in hashtable will not fit (must be a reduction in size). */
	return false;
    }

    const unsigned old_size = table->size;
    const size_t old_count = table->count;
    hash_node *const old_table = table->table;

    table->table = MALLOC(new_size * sizeof(hash_node));
    if (!table->table) {
	table->table = old_table;
	return false;
    }
    memset(table->table, 0, new_size * sizeof(hash_node));
    table->size = new_size;
    table->count = 0;

    for (unsigned i = 0; i < old_size; i++) {
	if (old_table[i].hash) {
	    bool inserted = false;
	    void **datum_location = insert(table, old_table[i].key, old_table[i].hash, &inserted);
	    if (!inserted || !datum_location) {
		FREE(table->table);
		table->table = old_table;
		table->size = old_size;
		table->count = old_count;
		return false;
	    }
	    *datum_location = old_table[i].datum;
	}
    }
    ASSERT(table->count == old_count);
    FREE(old_table);
    return true;
}

bool
hashtable2_verify(const hashtable2* table)
{
    ASSERT(table != NULL);

    size_t count = 0;
    const hash_node *node = table->table;
    const hash_node *end = table->table + table->size;
    for (; node != end; ++node) {
	if (node->hash) {
	    ++count;
	    /* TODO(farooq): additional validation here? */
	}
    }
    VERIFY(table->count == count);
    return true;
}

hashtable2_itor*
hashtable2_itor_new(hashtable2* table)
{
    ASSERT(table != NULL);

    hashtable2_itor* itor = MALLOC(sizeof(*itor));
    if (itor) {
	itor->table = table;
	itor->slot = -1;
    }
    return itor;
}

dict_itor*
hashtable2_dict_itor_new(hashtable2* table)
{
    ASSERT(table != NULL);

    dict_itor* itor = MALLOC(sizeof(*itor));
    if (itor) {
	if (!(itor->_itor = hashtable2_itor_new(table))) {
	    FREE(itor);
	    return NULL;
	}
	itor->_vtable = &hashtable2_itor_vtable;
    }
    return itor;
}

void
hashtable2_itor_free(hashtable2_itor* itor)
{
    ASSERT(itor != NULL);

    FREE(itor);
}

bool
hashtable2_itor_valid(const hashtable2_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->slot >= 0;
}

void
hashtable2_itor_invalidate(hashtable2_itor* itor)
{
    ASSERT(itor != NULL);

    itor->slot = -1;
}

bool
hashtable2_itor_next(hashtable2_itor* itor)
{
    ASSERT(itor != NULL);

    if (itor->slot < 0)
	return hashtable2_itor_first(itor);

    while (++itor->slot < (int) itor->table->size) {
	if (itor->table->table[itor->slot].hash) {
	    return true;
	}
    }
    itor->slot = -1;
    return false;
}

bool
hashtable2_itor_prev(hashtable2_itor* itor)
{
    ASSERT(itor != NULL);

    if (itor->slot < 0)
	return hashtable2_itor_last(itor);

    while (itor->slot-- > 0) {
	if (itor->table->table[itor->slot].hash) {
	    return true;
	}
    }
    ASSERT(itor->slot == -1);
    return false;
}

bool
hashtable2_itor_nextn(hashtable2_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!hashtable2_itor_next(itor))
	    return false;
    return itor->slot >= 0;
}

bool
hashtable2_itor_prevn(hashtable2_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!hashtable2_itor_prev(itor))
	    return false;
    return itor->slot >= 0;
}

bool
hashtable2_itor_first(hashtable2_itor* itor)
{
    ASSERT(itor != NULL);

    for (unsigned slot = 0; slot < itor->table->size; ++slot) {
	if (itor->table->table[slot].hash) {
	    itor->slot = slot;
	    return true;
	}
    }
    itor->slot = -1;
    return false;
}

bool
hashtable2_itor_last(hashtable2_itor* itor)
{
    ASSERT(itor != NULL);

    for (unsigned slot = itor->table->size; slot > 0;) {
	if (itor->table->table[--slot].hash) {
	    itor->slot = slot;
	    return true;
	}
    }
    itor->slot = -1;
    return false;
}

bool
hashtable2_itor_search(hashtable2_itor* itor, const void* key)
{
    const unsigned raw_hash = itor->table->hash_func(key);
    const unsigned hash = (raw_hash == 0) ? ~(unsigned)0 : raw_hash;
    const unsigned truncated_hash = hash % itor->table->size;
    unsigned index = truncated_hash;
    do {
	hash_node *node = &itor->table->table[index];
	if (!node->hash) {
	    break;
	}
	if (node->hash == hash && itor->table->cmp_func(key, node->key) == 0) {
	    itor->slot = index;
	    return true;
	}
	if (++index == itor->table->size) {
	    index = 0;
	}
    } while (index != truncated_hash);
    itor->slot = -1;
    return NULL;
}

const void*
hashtable2_itor_key(const hashtable2_itor* itor)
{
    ASSERT(itor != NULL);

    return (itor->slot >= 0) ? itor->table->table[itor->slot].key : NULL;
}

void**
hashtable2_itor_data(hashtable2_itor* itor)
{
    ASSERT(itor != NULL);

    return (itor->slot >= 0) ? &itor->table->table[itor->slot].datum : NULL;
}

