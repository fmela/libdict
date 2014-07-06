/*
 * libdict -- internal path reduction tree interface.
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

#ifndef _PR_TREE_H_
#define _PR_TREE_H_

#include "dict.h"

BEGIN_DECL

typedef struct pr_tree pr_tree;

pr_tree*	pr_tree_new(dict_compare_func cmp_func,
			    dict_delete_func del_func);
dict*		pr_dict_new(dict_compare_func cmp_func,
			    dict_delete_func del_func);
size_t		pr_tree_free(pr_tree* tree);
pr_tree*	pr_tree_clone(pr_tree* tree,
			      dict_key_datum_clone_func clone_func);

void**		pr_tree_insert(pr_tree* tree, void* key, bool* inserted);
void*		pr_tree_search(pr_tree* tree, const void* key);
bool		pr_tree_remove(pr_tree* tree, const void* key);
size_t		pr_tree_clear(pr_tree* tree);
size_t		pr_tree_traverse(pr_tree* tree, dict_visit_func visit);
size_t		pr_tree_count(const pr_tree* tree);
size_t		pr_tree_height(const pr_tree* tree);
size_t		pr_tree_mheight(const pr_tree* tree);
size_t		pr_tree_pathlen(const pr_tree* tree);
const void*	pr_tree_min(const pr_tree* tree);
const void*	pr_tree_max(const pr_tree* tree);
bool		pr_tree_verify(const pr_tree* tree);

typedef struct pr_itor pr_itor;

pr_itor*	pr_itor_new(pr_tree* tree);
dict_itor*	pr_dict_itor_new(pr_tree* tree);
void		pr_itor_free(pr_itor* tree);

bool		pr_itor_valid(const pr_itor* itor);
void		pr_itor_invalidate(pr_itor* itor);
bool		pr_itor_next(pr_itor* itor);
bool		pr_itor_prev(pr_itor* itor);
bool		pr_itor_nextn(pr_itor* itor, size_t count);
bool		pr_itor_prevn(pr_itor* itor, size_t count);
bool		pr_itor_first(pr_itor* itor);
bool		pr_itor_last(pr_itor* itor);
const void*	pr_itor_key(const pr_itor* itor);
void**		pr_itor_data(pr_itor* itor);
bool		pr_itor_remove(pr_itor* itor);

END_DECL

#endif /* !_PR_TREE_H_ */
