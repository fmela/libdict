/*
 * dict_private.h
 *
 * Private definitions for libdict.
 * Copyright (C) 2001-2010 Farooq Mela.
 *
 * $Id$
 */

#ifndef _DICT_PRIVATE_H_
#define _DICT_PRIVATE_H_

#include "dict.h"

#ifndef NDEBUG
# include <stdio.h>
# undef ASSERT
# if defined(__GNUC__)
#  define ASSERT(expr)														\
	if (!(expr))															\
		fprintf(stderr, "\n%s:%d (%s) assertion failed: `%s'\n",			\
				__FILE__, __LINE__, __PRETTY_FUNCTION__, #expr),			\
		abort()
# else
#  define ASSERT(expr)														\
	if (!(expr))															\
		fprintf(stderr, "\n%s:%d assertion failed: `%s'\n",					\
				__FILE__, __LINE__, #expr),									\
		abort()
# endif
#else
# define ASSERT(expr)
#endif

extern dict_malloc_func _dict_malloc;
extern dict_free_func _dict_free;
#define MALLOC(n)	(*_dict_malloc)(n)
#define FREE(p)		(*_dict_free)(p)

#define ABS(a)		((a) < 0 ? -(a) : +(a))
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#define MAX(a,b)	((a) > (b) ? (a) : (b))
#define SWAP(a,b,v)	v = (a), (a) = (b), (b) = v
#define UNUSED(p)	(void)&p

#if defined(__GNUC__)
# define GCC_INLINE		__inline__
# define GCC_UNUSED		__attribute__((__unused__))
# define GCC_CONST		__attribute__((__const__))
#else
# define GCC_INLINE
# define GCC_UNUSED
# define GCC_CONST
#endif

#endif /* !_DICT_PRIVATE_H_ */
