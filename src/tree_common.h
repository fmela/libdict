/*
 * libdict - common definitions for binary search trees.
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

#ifndef _TREE_COMMON_H_
#define _TREE_COMMON_H_

#include "dict.h"

#define TREE_NODE_FIELDS(node_type) \
    void*		key; \
    void*		datum; \
    node_type*		parent; \
    node_type*		llink; \
    node_type*		rlink;

typedef struct tree_node_base {
    TREE_NODE_FIELDS(struct tree_node_base);
} tree_node_base;

#define TREE_FIELDS(node_type) \
    node_type*		root; \
    size_t		count; \
    dict_compare_func	cmp_func; \
    dict_delete_func	del_func; \
    size_t		rotation_count;

typedef struct tree_base {
    TREE_FIELDS(struct tree_node_base);
} tree_base;

#define TREE_ITERATOR_FIELDS(tree_type, node_type) \
    tree_type*		tree; \
    node_type*		node;

/* Rotate |node| left.
 * |node| and |node->rlink| must not be NULL. */
void	    tree_node_rot_left(void *tree, void *node);
/* Rotate |node| right.
 * |node| and |node->llink| must not be NULL. */
void	    tree_node_rot_right(void *tree, void *node);
/* Return the predecessor of |node|, or NULL if |node| has no predecessor.
 * |node| must not be NULL. */
void*	    tree_node_prev(void *node);
/* Return the successor of |node|, or NULL if |node| has no successor.
 * |node| must not be NULL. */
void*	    tree_node_next(void *node);
/* Return the left child of |node|, or |node| if it has no right child.
 * |node| must not be NULL. */
void*	    tree_node_min(void *node);
/* Return the rightmost child of |node|, or |node| if it has no right child.
 * |node| must not be NULL. */
void*	    tree_node_max(void *node);
/* Return the data associated with the key, or NULL if not found. */
void*	    tree_search(void *tree, const void *key);
/* Return the node has the key, or NULL if not found. */
void*	    tree_search_node(void *tree, const void *key);
/* Return the data/node associated with the first key less than or
 * equal to the specified key, or NULL if not found. */
void*	    tree_search_le(void *tree, const void *key);
void*	    tree_search_le_node(void *tree, const void *key);
/* Return the data/node associated with the first key less than the
 * specified key, or NULL if not found. */
void*	    tree_search_lt(void *tree, const void *key);
void*	    tree_search_lt_node(void *tree, const void *key);
/* Return the data/node associated with the first key greater than or
 * equal to the specified key, or NULL if not found. */
void*	    tree_search_ge(void *tree, const void *key);
void*	    tree_search_ge_node(void *tree, const void *key);
/* Return the data/node associated with the first key greater than the
 * specified key, or NULL if not found. */
void*	    tree_search_gt(void *tree, const void *key);
void*	    tree_search_gt_node(void *tree, const void *key);
/* Return the minimal key in the tree, or NULL if the tree is empty. */
const void* tree_min(const void *tree);
/* Return the maximal key in the tree, or NULL if the tree is empty. */
const void* tree_max(const void *tree);
/* Traverses the tree in order, calling |visit| with each key and value pair,
 * stopping if |visit| returns false. Returns the number of times |visit| was
 * called. */
size_t	    tree_traverse(void *tree, dict_visit_func visit);
/* Return a count of the elements in |tree|. */
size_t	    tree_count(const void *tree);
/* Remove all elements from |tree|. */
size_t	    tree_clear(void *tree);
/* Remove all elements from |tree| and free its memory. */
size_t	    tree_free(void *tree);
/* Return a clone of the tree |tree| where |tree_size| is the tree object size
 * in bytes, |node_size| is the node object size in bytes, and |clone_func| is
 * an optional key-datum cloning function. */
void*	    tree_clone(void *tree, size_t tree_size, size_t node_size,
		       dict_key_datum_clone_func clone_func);
/* Returns the depth of the leaf with minimal depth, or 0 for an empty tree. */
size_t	    tree_min_leaf_depth(const void *tree);
/* Returns the depth of the leaf with maximal depth, or 0 for an empty tree. */
size_t	    tree_max_leaf_depth(const void *tree);

bool	    tree_iterator_valid(const void *iterator);
void	    tree_iterator_invalidate(void *iterator);
void	    tree_iterator_free(void *iterator);
bool	    tree_iterator_next(void *iterator);
bool	    tree_iterator_prev(void *iterator);
bool	    tree_iterator_next_n(void *iterator, size_t count);
bool	    tree_iterator_prev_n(void *iterator, size_t count);
bool	    tree_iterator_first(void *iterator);
bool	    tree_iterator_last(void *iterator);
bool	    tree_iterator_search(void *iterator, const void *key);
bool	    tree_iterator_search_le(void *iterator, const void *key);
bool	    tree_iterator_search_lt(void *iterator, const void *key);
bool	    tree_iterator_search_ge(void *iterator, const void *key);
bool	    tree_iterator_search_gt(void *iterator, const void *key);
const void* tree_iterator_key(const void *iterator);
void**	    tree_iterator_data(void *iterator);

#endif /* !defined(_TREE_COMMON_H_) */
