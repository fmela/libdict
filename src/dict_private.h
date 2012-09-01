/*
 * Copyright (C) 2001-2011 Farooq Mela.
 * All rights reserved.
 *
 * Private definitions for libdict.
 */

#ifndef _DICT_PRIVATE_H_
#define _DICT_PRIVATE_H_

#include <stdio.h>
#include <stdlib.h>

#include "dict.h"

#ifndef NDEBUG
# undef ASSERT
# if defined(__GNUC__)
#  define ASSERT(expr) \
    do { \
	if (!(expr)) { \
	    fprintf(stderr, "\n%s:%d (%s) assertion failed: `%s'\n", \
		    __FILE__, __LINE__, __PRETTY_FUNCTION__, #expr); \
	    abort(); \
	} \
    } while (0)
# else
#  define ASSERT(expr) \
    do { \
	if (!(expr)) { \
	    fprintf(stderr, "\n%s:%d assertion failed: `%s'\n", \
		    __FILE__, __LINE__, #expr); \
	    abort(); \
	} \
    } while (0)
# endif
#else
# define ASSERT(expr)
#endif

#define MALLOC(n)	(*dict_malloc_func)(n)
#define FREE(p)		(*dict_free_func)(p)

#define ABS(a)		((a) < 0 ? -(a) : (a))
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#define MAX(a,b)	((a) > (b) ? (a) : (b))
#define SWAP(a,b,v)	do { v = (a); (a) = (b); (b) = v; } while (0)

#if defined(__GNUC__)
# define GCC_INLINE	__inline__
# define GCC_CONST	__attribute__((__const__))
#else
# define GCC_INLINE
# define GCC_CONST
#endif

#endif /* !_DICT_PRIVATE_H_ */
