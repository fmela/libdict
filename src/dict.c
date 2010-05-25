/*
 * dict.c
 *
 * Implementation of generic dictionary routines.
 * Copyright (C) 2001-2010 Farooq Mela.
 *
 * $Id$
 */

#include <stdlib.h>

#include "dict.h"
#include "dict_private.h"

dict_malloc_func _dict_malloc = malloc;
dict_free_func _dict_free = free;

dict_malloc_func
dict_set_malloc(dict_malloc_func malloc_func)
{
	dict_malloc_func old = _dict_malloc;
	_dict_malloc = malloc_func ? malloc_func : malloc;
	return old;
}

dict_free_func
dict_set_free(dict_free_func func)
{
	dict_free_func old = _dict_free;
	_dict_free = func ? func : free;
	return old;
}

/* When comparing, we cannot just subtract because that might result in signed
 * overflow. */
int
dict_int_cmp(const void *k1, const void *k2)
{
	const int *a = k1, *b = k2;

	return (*a < *b) ? -1 : (*a > *b) ? +1 : 0;
}

int
dict_uint_cmp(const void *k1, const void *k2)
{
	const unsigned int *a = k1, *b = k2;

	return (*a < *b) ? -1 : (*a > *b) ? +1 : 0;
}

int
dict_long_cmp(const void *k1, const void *k2)
{
	const long *a = k1, *b = k2;

	return (*a < *b) ? -1 : (*a > *b) ? +1 : 0;
}

int
dict_ulong_cmp(const void *k1, const void *k2)
{
	const unsigned long *a = k1, *b = k2;

	return (*a < *b) ? -1 : (*a > *b) ? +1 : 0;
}

int
dict_ptr_cmp(const void *k1, const void *k2)
{
	return (k1 > k2) - (k1 < k2);
}

int
dict_str_cmp(const void *k1, const void *k2)
{
	const char *a = k1, *b = k2;
	char p, q;

	for (;;) {
		p = *a++; q = *b++;
		if (p == 0 || p != q)
			break;
	}
	return (p > q) - (p < q);
}

void
dict_destroy(dict *dct)
{
	ASSERT(dct != NULL);

	dct->_vtable->destroy(dct->_object);
	FREE(dct);
}

void
dict_itor_destroy(dict_itor *itor)
{
	ASSERT(itor != NULL);

	itor->_vtable->destroy(itor->_itor);
	FREE(itor);
}
