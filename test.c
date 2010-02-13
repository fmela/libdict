/* test.c
 * Some timing tests for libdict
 * Copyright (C) 2001-2010 Farooq Mela <farooq.mela@gmail.com> */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "dict.h"

const char appname[] = "test";

char *xstrdup(const char *str);

#ifdef __GNUC__
# define NORETURN	__attribute__((__noreturn__))
#else
# define NORETURN
#endif
void quit(const char *, ...) NORETURN;
void *xmalloc(size_t size);
void *xcalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
void *xdup(const void *ptr, size_t size);

unsigned s_hash(const unsigned char *p);
void shuffle(char **p, unsigned size);

unsigned
s_hash(const unsigned char *p)
{
	unsigned hash = 0;

	while (*p) {
		hash *= 31;
		hash += *p++;
	}
	return hash;
}

void
shuffle(char **p, unsigned size)
{
	unsigned i, n;
	char *t;

	for (i = 0; i < size - 1; i++) {
		n = rand() % (size - i);
		t = p[i+n]; p[i+n] = p[i]; p[i] = t;
	}
}

#define HSIZE		599

/*#define NWORDS	235881*/

#define NWORDS	98569

static size_t malloced = 0;

int
main(int argc, char **argv)
{
	unsigned i;
	char buf[512], *ptr, *p;
	char *words[NWORDS];
	FILE *fp;
	int rv;
	dict *dct;
	struct rusage start, end;
	struct timeval total = { 0, 0 };

	if (argc != 3) {
		fprintf(stderr, "usage: %s [type] [input]\n", appname);
		exit(EXIT_FAILURE);
	}

	srand((unsigned)time(NULL));

	dict_set_malloc(xmalloc);

	++argv;
	switch (argv[0][0]) {
	case 'h':
		dct = hb_dict_new(dict_str_cmp, free, NULL);
		break;
	case 'p':
		dct = pr_dict_new(dict_str_cmp, free, NULL);
		break;
	case 'r':
		dct = rb_dict_new(dict_str_cmp, free, NULL);
		break;
	case 't':
		dct = tr_dict_new(dict_str_cmp, free, NULL);
		break;
	case 's':
		dct = sp_dict_new(dict_str_cmp, free, NULL);
		break;
	case 'w':
		dct = wb_dict_new(dict_str_cmp, free, NULL);
		break;
	case 'H':
		dct = hashtable_dict_new(dict_str_cmp,
								 (dict_hsh_func)s_hash, free, NULL, HSIZE);
		break;
	default:
		quit("type must be one of h, p, r, t, s, w or H");
	}

	if (!dct)
		quit("can't create container");

	fp = fopen(argv[1], "r");
	if (fp == NULL)
		quit("cant open file `%s': %s", argv[1], strerror(errno));

	i = 0;
	for (i = 0; i < NWORDS && fgets(buf, sizeof(buf), fp); i++) {
		strtok(buf, "\n");
		words[i] = xstrdup(buf);
	}
	fclose(fp);

	malloced = 0;
	getrusage(RUSAGE_SELF, &start);
	for (i = 0; i < NWORDS; i++) {
		ptr = words[i];
		if ((rv = dict_insert(dct, ptr, ptr, 0)) != 0)
			quit("insert failed with %d for `%s'", rv, ptr);
	}
	getrusage(RUSAGE_SELF, &end);
	if (end.ru_utime.tv_usec < start.ru_utime.tv_usec)
		end.ru_utime.tv_usec += 1000000, end.ru_utime.tv_sec--;
	end.ru_utime.tv_usec -= start.ru_utime.tv_usec;
	end.ru_utime.tv_sec -= start.ru_utime.tv_sec;
	total.tv_sec += end.ru_utime.tv_sec;
	if ((total.tv_usec += end.ru_utime.tv_usec) > 1000000)
		total.tv_usec -= 1000000, total.tv_sec++;
	printf("insert = %02f s\n",
		   (end.ru_utime.tv_sec * 1000000 + end.ru_utime.tv_usec) / 1000000.0);
/*	printf("memory used = %u bytes\n", malloced); */

	if ((i = dict_count(dct)) != NWORDS)
		quit("bad count (%u)!", i);

	getrusage(RUSAGE_SELF, &start);
	for (i = 0; i < NWORDS; i++) {
		ptr = words[i];
		if ((p = dict_search(dct, ptr)) == NULL)
			quit("lookup failed for `%s'", buf);
		if (p != ptr)
			quit("bad data for `%s', got `%s' instead", ptr, p);
	}
	getrusage(RUSAGE_SELF, &end);
	if (end.ru_utime.tv_usec < start.ru_utime.tv_usec)
		end.ru_utime.tv_usec += 1000000, end.ru_utime.tv_sec--;
	end.ru_utime.tv_usec -= start.ru_utime.tv_usec;
	end.ru_utime.tv_sec -= start.ru_utime.tv_sec;
	total.tv_sec += end.ru_utime.tv_sec;
	if ((total.tv_usec += end.ru_utime.tv_usec) > 1000000)
		total.tv_usec -= 1000000, total.tv_sec++;
	printf("search = %02f s\n",
		   (end.ru_utime.tv_sec * 1000000 + end.ru_utime.tv_usec) / 1000000.0);

	getrusage(RUSAGE_SELF, &start);
	for (i = 0; i < NWORDS; i++) {
		ptr = words[i];
		if ((rv = dict_remove(dct, ptr, TRUE)) != 0)
			quit("removing `%s' failed (%d)!\n", ptr, rv);
	}
	getrusage(RUSAGE_SELF, &end);
	if (end.ru_utime.tv_usec < start.ru_utime.tv_usec)
		end.ru_utime.tv_usec += 1000000, end.ru_utime.tv_sec--;
	end.ru_utime.tv_usec -= start.ru_utime.tv_usec;
	end.ru_utime.tv_sec -= start.ru_utime.tv_sec;
	total.tv_sec += end.ru_utime.tv_sec;
	if ((total.tv_usec += end.ru_utime.tv_usec) > 1000000)
		total.tv_usec -= 1000000, total.tv_sec++;
	printf("remove = %02f s\n",
		   (end.ru_utime.tv_sec * 1000000 + end.ru_utime.tv_usec) / 1000000.0);

	if ((i = dict_count(dct)) != 0)
		quit("error - count not zero (%u)!", i);

	dict_destroy(dct, TRUE);

	printf(" total = %02f s\n",
		   (total.tv_sec * 1000000 + total.tv_usec) / 1000000.0);

	exit(EXIT_SUCCESS);
}

char *
xstrdup(const char *str)
{
	return xdup(str, strlen(str) + 1);
}

void
quit(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "%s: ", appname);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);

	exit(EXIT_FAILURE);
}

void *
xmalloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL)
		quit("out of memory");
	malloced += size;
	return p;
}

void *
xcalloc(size_t size)
{
	void *p;

	p = xmalloc(size);
	memset(p, 0, size);
	return p;
}

void *
xrealloc(void *ptr, size_t size)
{
	void *p;

	if ((p = realloc(ptr, size)) == NULL && size != 0)
		quit("out of memory");
	return p;
}

void *
xdup(const void *ptr, size_t size)
{
	void *p;

	p = xmalloc(size);
	memcpy(p, ptr, size);
	return p;
}
