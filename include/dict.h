/*
 * dict.h
 *
 * Interface for generic access to dictionary library.
 * Copyright (C) 2001-2008 Farooq Mela.
 *
 * $Id$
 */

#ifndef _DICT_H_
#define _DICT_H_

#include <stddef.h>

#define DICT_VERSION_MAJOR		0
#define DICT_VERSION_MINOR		3
#define DICT_VERSION_PATCH		0

#ifndef FALSE
#define FALSE	0
#endif

#ifndef TRUE
#define TRUE	(!FALSE)
#endif

#if defined(__cplusplus) || defined(c_plusplus)
# define BEGIN_DECL	extern "C" {
# define END_DECL	}
#else
# define BEGIN_DECL
# define END_DECL
#endif

BEGIN_DECL

typedef void *(*dict_malloc_func)(size_t);
typedef void  (*dict_free_func)(void *);

dict_malloc_func	dict_set_malloc(dict_malloc_func func);
dict_free_func		dict_set_free(dict_free_func func);

typedef int			(*dict_cmp_func)(const void *, const void *);
typedef void		(*dict_del_func)(void *);
typedef int			(*dict_vis_func)(const void *, void *);
typedef unsigned	(*dict_hsh_func)(const void *);

typedef struct dict			dict;
typedef struct dict_itor	dict_itor;

typedef dict_itor	*(*inew_func)	(void *obj);
typedef void		 (*destroy_func)(void *obj, int del);
typedef int			 (*insert_func)	(void *obj, void *k, void *d, int ow);
typedef int			 (*probe_func)	(void *obj, void *key, void **dat);
typedef void		*(*search_func)	(void *obj, const void *key);
typedef const void	*(*csearch_func)(const void *obj, const void *key);
typedef int			 (*remove_func)	(void *obj, const void *key, int del);
typedef void		 (*empty_func)	(void *obj, int del);
typedef void		 (*walk_func)	(void *obj, dict_vis_func visit);
typedef unsigned	 (*count_func)	(const void *obj);

struct dict_vtable {
	inew_func		 inew;
	destroy_func	 destroy;
	insert_func		 insert;
	probe_func		 probe;
	search_func		 search;
	csearch_func	 csearch;
	remove_func		 remove;
	empty_func		 empty;
	walk_func		 walk;
	count_func		 count;
};

typedef void		 (*idestroy_func)	(void *itor);
typedef int			 (*valid_func)		(const void *itor);
typedef void		 (*invalidate_func)	(void *itor);
typedef int			 (*next_func)		(void *itor);
typedef int			 (*prev_func)		(void *itor);
typedef int			 (*nextn_func)		(void *itor, unsigned count);
typedef int			 (*prevn_func)		(void *itor, unsigned count);
typedef int			 (*first_func)		(void *itor);
typedef int			 (*last_func)		(void *itor);
typedef void		*(*key_func)		(void *itor);
typedef void		*(*data_func)		(void *itor);
typedef const void	*(*cdata_func)		(const void *itor);
typedef int			 (*dataset_func)	(void *itor, void *dat, int del);
typedef int			 (*iremove_func)	(void *itor, int del);
typedef int			 (*compare_func)	(void *itor1, void *itor2);

struct itor_vtable {
	idestroy_func	 destroy;
	valid_func		 valid;
	invalidate_func	 invalid;
	next_func		 next;
	prev_func		 prev;
	nextn_func		 nextn;
	prevn_func		 prevn;
	first_func		 first;
	last_func		 last;
	key_func		 key;
	data_func		 data;
	cdata_func		 cdata;
	dataset_func	 dataset;
	iremove_func	 remove;
	compare_func	 compare;
};

struct dict {
	void				*_object;
	struct dict_vtable	*_vtable;
};

#define dict_private(dct)		(dct)->_object
#define dict_insert(dct,k,d,o)	(dct)->_vtable->insert((dct)->_object, (k), (d), (o))
#define dict_probe(dct,k,d)		(dct)->_vtable->probe((dct)->_object, (k), (d))
#define dict_search(dct,k)		(dct)->_vtable->search((dct)->_object, (k))
#define dict_csearch(dct,k)		(dct)->_vtable->csearch((dct)->_object, (k))
#define dict_remove(dct,k,del)	(dct)->_vtable->remove((dct)->_object, (k), (del))
#define dict_walk(dct,f)		(dct)->_vtable->walk((dct)->_object, (f))
#define dict_count(dct)			(dct)->_vtable->count((dct)->_object)
#define dict_empty(dct,d)		(dct)->_vtable->empty((dct)->_object, (d))
void dict_destroy(dict *dct, int del);
#define dict_itor_new(dct)		(dct)->_vtable->inew((dct)->_object)

struct dict_itor {
	void				*_itor;
	struct itor_vtable	*_vtable;
};

#define dict_itor_private(i)		(i)->_itor
#define dict_itor_valid(i)			(i)->_vtable->valid((i)->_itor)
#define dict_itor_invalidate(i)		(i)->_vtable->invalid((i)->_itor)
#define dict_itor_next(i)			(i)->_vtable->next((i)->_itor)
#define dict_itor_prev(i)			(i)->_vtable->prev((i)->_itor)
#define dict_itor_nextn(i,n)		(i)->_vtable->nextn((i)->_itor, (n))
#define dict_itor_prevn(i,n)		(i)->_vtable->prevn((i)->_itor, (n))
#define dict_itor_first(i)			(i)->_vtable->first((i)->_itor)
#define dict_itor_last(i)			(i)->_vtable->last((i)->_itor)
#define dict_itor_search(i,k)		(i)->_vtable->search((i)->_itor, (k))
#define dict_itor_key(i)			(i)->_vtable->key((i)->_itor)
#define dict_itor_data(i)			(i)->_vtable->data((i)->_itor)
#define dict_itor_cdata(i)			(i)->_vtable->cdata((i)->_itor)
#define dict_itor_set_data(i,dat,d)	(i)->_vtable->setdata((i)->_itor, (dat), (d))
#define dict_itor_remove(i)			(i)->_vtable->remove((i)->_itor)
void dict_itor_destroy(dict_itor *itor);

int		dict_int_cmp(const void *k1, const void *k2);
int		dict_uint_cmp(const void *k1, const void *k2);
int		dict_long_cmp(const void *k1, const void *k2);
int		dict_ulong_cmp(const void *k1, const void *k2);
int		dict_ptr_cmp(const void *k1, const void *k2);
int		dict_str_cmp(const void *k1, const void *k2);

END_DECL

#include "hashtable.h"
#include "hb_tree.h"
#include "pr_tree.h"
#include "rb_tree.h"
#include "sp_tree.h"
#include "tr_tree.h"
#include "wb_tree.h"

#endif /* !_DICT_H_ */
