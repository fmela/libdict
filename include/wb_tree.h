/*
 * libdict -- weight balanced tree interface.
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

#ifndef _WB_TREE_H_
#define _WB_TREE_H_

#include "dict.h"

BEGIN_DECL

typedef struct wb_tree wb_tree;

wb_tree*	wb_tree_new(dict_compare_func cmp_func,
			    dict_delete_func del_func);
dict*		wb_dict_new(dict_compare_func cmp_func,
			    dict_delete_func del_func);
size_t		wb_tree_free(wb_tree* tree);
wb_tree*	wb_tree_clone(wb_tree* tree,
			      dict_key_datum_clone_func clone_func);

void**		wb_tree_insert(wb_tree* tree, void* key, bool* inserted);
void*		wb_tree_search(wb_tree* tree, const void* key);
bool		wb_tree_remove(wb_tree* tree, const void* key);
size_t		wb_tree_clear(wb_tree* tree);
size_t		wb_tree_traverse(wb_tree* tree, dict_visit_func visit);
size_t		wb_tree_count(const wb_tree* tree);
size_t		wb_tree_height(const wb_tree* tree);
size_t		wb_tree_mheight(const wb_tree* tree);
size_t		wb_tree_pathlen(const wb_tree* tree);
const void*	wb_tree_min(const wb_tree* tree);
const void*	wb_tree_max(const wb_tree* tree);
bool		wb_tree_verify(const wb_tree* tree);

typedef struct wb_itor wb_itor;

wb_itor*	wb_itor_new(wb_tree* tree);
dict_itor*	wb_dict_itor_new(wb_tree* tree);
void		wb_itor_free(wb_itor* tree);

bool		wb_itor_valid(const wb_itor* itor);
void		wb_itor_invalidate(wb_itor* itor);
bool		wb_itor_next(wb_itor* itor);
bool		wb_itor_prev(wb_itor* itor);
bool		wb_itor_nextn(wb_itor* itor, size_t count);
bool		wb_itor_prevn(wb_itor* itor, size_t count);
bool		wb_itor_first(wb_itor* itor);
bool		wb_itor_last(wb_itor* itor);
const void*	wb_itor_key(const wb_itor* itor);
void**		wb_itor_data(wb_itor* itor);
bool		wb_itor_remove(wb_itor* itor);

END_DECL

#endif /* !_WB_TREE_H_ */
