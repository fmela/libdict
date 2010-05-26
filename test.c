/* test.c
 * Some timing tests for libdict
 * Copyright (C) 2001-2010 Farooq Mela <farooq.mela@gmail.com> */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
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

static int hash_count = 0;
unsigned
s_hash(const unsigned char *p)
{
	unsigned hash = 0;

	hash_count++;

	while (*p)
		hash = hash*33 + *p++;
	return hash;
}

static int comp_count = 0;
int
my_str_cmp(const void *k1, const void *k2)
{
	comp_count++;
	return strcmp(k1, k2);
}

void
key_str_free(void *key, void *datum)
{
	assert(key == datum);
	free(key);
}

/* #define HSIZE		599 */
/* #define HSIZE		997 */
#define HSIZE 9973

static size_t malloced = 0;

int
main(int argc, char **argv)
{
	unsigned i, nwords;
	char buf[512], *p;
	char **words;
	FILE *fp;
	int rv;
	dict *dct;
	struct rusage start, end;
	struct timeval total = { 0, 0 };
	int total_comp, total_hash;

	if (argc != 3) {
		fprintf(stderr, "usage: %s [type] [input]\n", appname);
		exit(EXIT_FAILURE);
	}

	srand((unsigned)time(NULL));

	dict_set_malloc(xmalloc);

	++argv;
	switch (argv[0][0]) {
	case 'h':
		dct = hb_dict_new(my_str_cmp, key_str_free);
		break;
	case 'p':
		dct = pr_dict_new(my_str_cmp, key_str_free);
		break;
	case 'r':
		dct = rb_dict_new(my_str_cmp, key_str_free);
		break;
	case 't':
		dct = tr_dict_new(my_str_cmp, key_str_free);
		break;
	case 's':
		dct = sp_dict_new(my_str_cmp, key_str_free);
		break;
	case 'w':
		dct = wb_dict_new(my_str_cmp, key_str_free);
		break;
	case 'H':
		dct = hashtable_dict_new(my_str_cmp,
								 (dict_hash_func)s_hash, key_str_free, HSIZE);
		break;
	default:
		quit("type must be one of h, p, r, t, s, w or H");
	}

	if (!dct)
		quit("can't create container");

	fp = fopen(argv[1], "r");
	if (fp == NULL)
		quit("cant open file `%s': %s", argv[1], strerror(errno));

	for (nwords = 0; fgets(buf, sizeof(buf), fp); nwords++)
		;

	if (nwords == 0)
		quit("nothing read from file");

	printf("Processing %u words\n", nwords);
	words = xmalloc(sizeof(*words) * nwords);

	rewind(fp);
	for (i = 0; i < nwords && fgets(buf, sizeof(buf), fp); i++) {
		strtok(buf, "\n");
		words[i] = xstrdup(buf);
	}
	fclose(fp);

	total_comp = total_hash = 0;

	malloced = 0;
	comp_count = hash_count = 0;
	getrusage(RUSAGE_SELF, &start);
	for (i = 0; i < nwords; i++) {
		if ((rv = dict_insert(dct, words[i], words[i], 0)) != 0)
			quit("insert failed with %d for `%s'", rv, words[i]);
	}
	getrusage(RUSAGE_SELF, &end);
	if (end.ru_utime.tv_usec < start.ru_utime.tv_usec)
		end.ru_utime.tv_usec += 1000000, end.ru_utime.tv_sec--;
	end.ru_utime.tv_usec -= start.ru_utime.tv_usec;
	end.ru_utime.tv_sec -= start.ru_utime.tv_sec;
	total.tv_sec += end.ru_utime.tv_sec;
	if ((total.tv_usec += end.ru_utime.tv_usec) > 1000000)
		total.tv_usec -= 1000000, total.tv_sec++;
	printf("      inserts %02f s (%8d cmp, %8d hash)\n",
		   (end.ru_utime.tv_sec * 1000000 + end.ru_utime.tv_usec) / 1000000.0, comp_count, hash_count);
	total_comp += comp_count; comp_count = 0;
	total_hash += hash_count; hash_count = 0;
/*	printf("memory used = %u bytes\n", malloced); */

	if ((i = dict_count(dct)) != nwords)
		quit("bad count (%u)!", i);

	/* shuffle(words, nwords); */

	comp_count = hash_count = 0;
	getrusage(RUSAGE_SELF, &start);
	for (i = 0; i < nwords; i++) {
		if ((p = dict_search(dct, words[i])) == NULL)
			quit("lookup failed for `%s'", buf);
		if (p != words[i])
			quit("bad data for `%s', got `%s' instead", words[i], p);
	}
	getrusage(RUSAGE_SELF, &end);
	if (end.ru_utime.tv_usec < start.ru_utime.tv_usec)
		end.ru_utime.tv_usec += 1000000, end.ru_utime.tv_sec--;
	end.ru_utime.tv_usec -= start.ru_utime.tv_usec;
	end.ru_utime.tv_sec -= start.ru_utime.tv_sec;
	total.tv_sec += end.ru_utime.tv_sec;
	if ((total.tv_usec += end.ru_utime.tv_usec) > 1000000)
		total.tv_usec -= 1000000, total.tv_sec++;
	printf("good searches %02f s (%8d cmp, %8d hash)\n",
		   (end.ru_utime.tv_sec * 1000000 + end.ru_utime.tv_usec) / 1000000.0, comp_count, hash_count);
	total_comp += comp_count; comp_count = 0;
	total_hash += hash_count; hash_count = 0;

	comp_count = hash_count = 0;
	getrusage(RUSAGE_SELF, &start);
	for (i = 0; i < nwords; i++) {
		rv = rand() % strlen(words[i]);
		words[i][rv]++;
		dict_search(dct, words[i]);
		words[i][rv]--;
	}
	getrusage(RUSAGE_SELF, &end);
	if (end.ru_utime.tv_usec < start.ru_utime.tv_usec)
		end.ru_utime.tv_usec += 1000000, end.ru_utime.tv_sec--;
	end.ru_utime.tv_usec -= start.ru_utime.tv_usec;
	end.ru_utime.tv_sec -= start.ru_utime.tv_sec;
	total.tv_sec += end.ru_utime.tv_sec;
	if ((total.tv_usec += end.ru_utime.tv_usec) > 1000000)
		total.tv_usec -= 1000000, total.tv_sec++;
	printf(" bad searches %02f s (%8d cmp, %8d hash)\n",
		   (end.ru_utime.tv_sec * 1000000 + end.ru_utime.tv_usec) / 1000000.0, comp_count, hash_count);
	total_comp += comp_count; comp_count = 0;
	total_hash += hash_count; hash_count = 0;

	/* shuffle(words, nwords); */

	getrusage(RUSAGE_SELF, &start);
	for (i = 0; i < nwords; i++) {
		if ((rv = dict_remove(dct, words[i])) != 0)
			quit("removing `%s' failed (%d)!\n", words[i], rv);
	/*	wb_tree_verify(dct->_object); */
	}
	getrusage(RUSAGE_SELF, &end);
	if (end.ru_utime.tv_usec < start.ru_utime.tv_usec)
		end.ru_utime.tv_usec += 1000000, end.ru_utime.tv_sec--;
	end.ru_utime.tv_usec -= start.ru_utime.tv_usec;
	end.ru_utime.tv_sec -= start.ru_utime.tv_sec;
	total.tv_sec += end.ru_utime.tv_sec;
	if ((total.tv_usec += end.ru_utime.tv_usec) > 1000000)
		total.tv_usec -= 1000000, total.tv_sec++;
	printf(" removes took %02f s (%8d cmp, %8d hash)\n",
		   (end.ru_utime.tv_sec * 1000000 + end.ru_utime.tv_usec) / 1000000.0, comp_count, hash_count);
	total_comp += comp_count; comp_count = 0;
	total_hash += hash_count; hash_count = 0;

	if ((i = dict_count(dct)) != 0)
		quit("error - count not zero (%u)!", i);

	dict_destroy(dct);

	printf("       totals %02f s (%8d cmp, %8d hash)\n",
		   (total.tv_sec * 1000000 + total.tv_usec) / 1000000.0, total_comp, total_hash);

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
