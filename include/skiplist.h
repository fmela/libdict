/*
 * libdict -- skiplist interface.
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

#ifndef _SKIPLIST_H_
#define _SKIPLIST_H_

#include "dict.h"

BEGIN_DECL

typedef struct skiplist skiplist;

skiplist*	skiplist_new(dict_compare_func cmp_func,
			     dict_delete_func del_func,
			     unsigned max_link);
dict*		skiplist_dict_new(dict_compare_func cmp_func,
				  dict_delete_func del_func,
				  unsigned max_link);
size_t		skiplist_free(skiplist* list);
skiplist*	skiplist_clone(skiplist* list,
			       dict_key_datum_clone_func clone_func);

void**		skiplist_insert(skiplist* list, void* key, bool* inserted);
void*		skiplist_search(skiplist* list, const void* key);
bool		skiplist_remove(skiplist* list, const void* key);
size_t		skiplist_clear(skiplist* list);
size_t		skiplist_traverse(skiplist* list, dict_visit_func visit);
size_t		skiplist_count(const skiplist* list);
bool		skiplist_verify(const skiplist* list);

typedef struct skiplist_itor skiplist_itor;

skiplist_itor*	skiplist_itor_new(skiplist* list);
dict_itor*	skiplist_dict_itor_new(skiplist* list);
void		skiplist_itor_free(skiplist_itor* );

bool		skiplist_itor_valid(const skiplist_itor* itor);
void		skiplist_itor_invalidate(skiplist_itor* itor);
bool		skiplist_itor_next(skiplist_itor* itor);
bool		skiplist_itor_prev(skiplist_itor* itor);
bool		skiplist_itor_nextn(skiplist_itor* itor, size_t count);
bool		skiplist_itor_prevn(skiplist_itor* itor, size_t count);
bool		skiplist_itor_first(skiplist_itor* itor);
bool		skiplist_itor_last(skiplist_itor* itor);
bool		skiplist_itor_search(skiplist_itor* itor, const void* key);
const void*	skiplist_itor_key(const skiplist_itor* itor);
void**		skiplist_itor_data(skiplist_itor* itor);
bool		skiplist_itor_remove(skiplist_itor* itor);

END_DECL

#endif /* !_SKIPLIST_H_ */
