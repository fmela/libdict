/*
 * libdict -- generic dictionary implementation.
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
dict_free(dict *dct)
{
	ASSERT(dct != NULL);

	dct->_vtable->dfree(dct->_object);
	FREE(dct);
}

void
dict_itor_free(dict_itor *itor)
{
	ASSERT(itor != NULL);

	itor->_vtable->ifree(itor->_itor);
	FREE(itor);
}
