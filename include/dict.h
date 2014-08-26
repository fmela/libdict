/*
 * libdict -- generic interface definitions.
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

#ifndef _DICT_H_
#define _DICT_H_

#if defined(__cplusplus) || defined(c_plusplus)
# define BEGIN_DECL     extern "C" {
# define END_DECL       }
#else
# define BEGIN_DECL
# define END_DECL
#endif

BEGIN_DECL

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define DICT_VERSION_MAJOR	0
#define DICT_VERSION_MINOR	3
#define DICT_VERSION_PATCH	0
extern const char* const kDictVersionString;

/* A pointer to a function that compares two keys. It needs to return a
 * negative value if k1<k2, a positive value if k1>k2, and zero if the keys are
 * equal. The comparison should be reflexive (k1>k2 implies k1<k2, etc.),
 * symmetric (k1=k1) and transitive (k1>k2 and k2>k3 implies k1>k3). */
typedef int	    (*dict_compare_func)(const void*, const void*);
/* A pointer to a function that is called when a key-value pair gets removed
 * from a dictionary. */
typedef void	    (*dict_delete_func)(void*, void*);
/* A pointer to a function used for iterating over dictionary contents. */
typedef bool	    (*dict_visit_func)(const void*, void*);
/* A pointer to a function that returns the hash value of a key. */
typedef unsigned    (*dict_hash_func)(const void*);
/* A pointer to a function that returns the priority of a key. */
typedef unsigned    (*dict_prio_func)(const void*);
/* A pointer to a function that clones a key or datum value, given the key-datum
 * pair. */
typedef void	    (*dict_key_datum_clone_func)(void** key, void** datum);
/* A pointer to a function that clones a dictionary. */
typedef void*	    (*dict_clone_func)(void*,
				       dict_key_datum_clone_func clone_func);


/* A pointer to a function that libdict will use to allocate memory. */
extern void*	    (*dict_malloc_func)(size_t);
/* A pointer to a function that libdict will use to deallocate memory. */
extern void	    (*dict_free_func)(void*);

/* Forward declarations for transparent type dict_itor. */
typedef struct dict_itor dict_itor;

typedef dict_itor*  (*dict_inew_func)(void* obj);
typedef size_t      (*dict_dfree_func)(void* obj);
typedef void**	    (*dict_insert_func)(void* obj, void* key, bool* inserted);
typedef void*       (*dict_search_func)(void* obj, const void* key);
typedef bool	    (*dict_remove_func)(void* obj, const void* key);
typedef size_t      (*dict_clear_func)(void* obj);
typedef size_t      (*dict_traverse_func)(void* obj, dict_visit_func visit);
typedef size_t      (*dict_count_func)(const void* obj);
typedef bool	    (*dict_verify_func)(const void* obj);

typedef struct {
    dict_inew_func      inew;
    dict_dfree_func     dfree;
    dict_insert_func    insert;
    dict_search_func    search;
    dict_search_func    search_le;
    dict_search_func    search_lt;
    dict_search_func    search_ge;
    dict_search_func    search_gt;
    dict_remove_func    remove;
    dict_clear_func     clear;
    dict_traverse_func  traverse;
    dict_count_func     count;
    dict_verify_func	verify;
    dict_clone_func	clone;
} dict_vtable;

typedef void	    (*dict_ifree_func)(void* itor);
typedef bool	    (*dict_valid_func)(const void* itor);
typedef void	    (*dict_invalidate_func)(void* itor);
typedef bool	    (*dict_next_func)(void* itor);
typedef bool	    (*dict_prev_func)(void* itor);
typedef bool	    (*dict_nextn_func)(void* itor, size_t count);
typedef bool	    (*dict_prevn_func)(void* itor, size_t count);
typedef bool	    (*dict_first_func)(void* itor);
typedef bool	    (*dict_last_func)(void* itor);
typedef void*	    (*dict_key_func)(void* itor);
typedef void**	    (*dict_data_func)(void* itor);
typedef bool	    (*dict_isearch_func)(void* itor, const void* key);
typedef bool	    (*dict_iremove_func)(void* itor);
typedef int	    (*dict_icompare_func)(void* itor1, void* itor2);

typedef struct {
    dict_ifree_func	    ifree;
    dict_valid_func	    valid;
    dict_invalidate_func    invalid;
    dict_next_func	    next;
    dict_prev_func	    prev;
    dict_nextn_func	    nextn;
    dict_prevn_func	    prevn;
    dict_first_func	    first;
    dict_last_func	    last;
    dict_key_func	    key;
    dict_data_func	    data;
    dict_isearch_func       search;
    dict_isearch_func       search_le;
    dict_isearch_func       search_lt;
    dict_isearch_func       search_ge;
    dict_isearch_func       search_gt;
    dict_iremove_func       remove;
    dict_icompare_func      compare;
} itor_vtable;

typedef struct {
    void*	    _object;
    dict_vtable*    _vtable;
} dict;

#define dict_private(dct)	    ((dct)->_object)
#define dict_insert(dct,k,d)	    ((dct)->_vtable->insert((dct)->_object,(k),(d)))
#define dict_search(dct,k)	    ((dct)->_vtable->search((dct)->_object, (k)))
#define dict_has_near_search(dct)   ((dct)->_vtable->search_le != NULL)	/* Assumes all-or-none near search functions. */
#define dict_search_le(dct,k)	    ((dct)->_vtable->search_le((dct)->_object, (k)))
#define dict_search_lt(dct,k)	    ((dct)->_vtable->search_lt((dct)->_object, (k)))
#define dict_search_ge(dct,k)	    ((dct)->_vtable->search_ge((dct)->_object, (k)))
#define dict_search_gt(dct,k)	    ((dct)->_vtable->search_gt((dct)->_object, (k)))
#define dict_remove(dct,k)	    ((dct)->_vtable->remove((dct)->_object, (k)))
#define dict_traverse(dct,f)	    ((dct)->_vtable->traverse((dct)->_object, (f)))
#define dict_count(dct)		    ((dct)->_vtable->count((dct)->_object))
#define dict_verify(dct)	    ((dct)->_vtable->verify((dct)->_object))
#define dict_clear(dct)		    ((dct)->_vtable->clear((dct)->_object))
#define dict_itor_new(dct)	    (dct)->_vtable->inew((dct)->_object)
size_t dict_free(dict* dct);
dict* dict_clone(dict* dct, dict_key_datum_clone_func clone_func);

struct dict_itor {
    void*	    _itor;
    itor_vtable*    _vtable;
};

#define dict_itor_private(i)	    ((i)->_itor)
#define dict_itor_valid(i)	    ((i)->_vtable->valid((i)->_itor))
#define dict_itor_invalidate(i)	    ((i)->_vtable->invalid((i)->_itor))
#define dict_itor_next(i)	    ((i)->_vtable->next((i)->_itor))
#define dict_itor_prev(i)	    ((i)->_vtable->prev((i)->_itor))
#define dict_itor_nextn(i,n)	    ((i)->_vtable->nextn((i)->_itor, (n)))
#define dict_itor_prevn(i,n)	    ((i)->_vtable->prevn((i)->_itor, (n)))
#define dict_itor_first(i)	    ((i)->_vtable->first((i)->_itor))
#define dict_itor_last(i)	    ((i)->_vtable->last((i)->_itor))
#define dict_itor_search(i,k)	    ((i)->_vtable->search((i)->_itor, (k)))
#define dict_itor_search_le(i,k)    ((i)->_vtable->search_le((i)->_itor, (k)))
#define dict_itor_search_lt(i,k)    ((i)->_vtable->search_lt((i)->_itor, (k)))
#define dict_itor_search_ge(i,k)    ((i)->_vtable->search_ge((i)->_itor, (k)))
#define dict_itor_search_gt(i,k)    ((i)->_vtable->search_gt((i)->_itor, (k)))
#define dict_itor_key(i)	    ((i)->_vtable->key((i)->_itor))
#define dict_itor_data(i)	    ((i)->_vtable->data((i)->_itor))
#define dict_itor_remove(i)	    ((i)->_vtable->remove((i)->_itor))
void dict_itor_free(dict_itor* itor);

int dict_int_cmp(const void* k1, const void* k2);
int dict_uint_cmp(const void* k1, const void* k2);
int dict_long_cmp(const void* k1, const void* k2);
int dict_ulong_cmp(const void* k1, const void* k2);
int dict_ptr_cmp(const void* k1, const void* k2);
int dict_str_cmp(const void* k1, const void* k2);
unsigned dict_str_hash(const void* str);

END_DECL

#include "hashtable.h"
#include "hashtable2.h"
#include "hb_tree.h"
#include "pr_tree.h"
#include "rb_tree.h"
#include "skiplist.h"
#include "sp_tree.h"
#include "tr_tree.h"
#include "wb_tree.h"

#endif /* !_DICT_H_ */
