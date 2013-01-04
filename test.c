/* test.c
 * Some timing tests for libdict
 * Copyright (C) 2001-2011 Farooq Mela */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "dict.h"
#include "dict_private.h"
#include "tree_common.h"

const char appname[] = "test";

char *xstrdup(const char *str);

#ifdef __GNUC__
# define NORETURN	__attribute__((__noreturn__))
#else
# define NORETURN
#endif
void quit(const char *, ...) NORETURN;
void warn(const char *fmt, ...);
void *xmalloc(size_t size);

static size_t hash_count = 0, comp_count = 0;
unsigned str_hash(const void *p);
int my_strcmp(const void *k1, const void *k2);
unsigned ptr_hash(const void *p);
int my_ptrcmp(const void *k1, const void *k2);
void shuffle(char **p, unsigned size);
void key_str_free(void *key, void *datum);

void timer_start(struct rusage* start);
void timer_end(const struct rusage* start, struct rusage *end,
	       struct timeval *total);

/* #define HSIZE 599 */
/* #define HSIZE 997 */
#define HSIZE 9973

static size_t malloced = 0;

int
main(int argc, char **argv)
{
    if (argc != 3) {
	fprintf(stderr, "usage: %s [type] [input]\n", appname);
	fprintf(stderr, "type: specifies the dictionary type:\n");
	fprintf(stderr, "   h: height-balanced tree\n");
	fprintf(stderr, "   p: path-reduction tree\n");
	fprintf(stderr, "   r: red-black tree\n");
	fprintf(stderr, "   t: treap\n");
	fprintf(stderr, "   s: splay tree\n");
	fprintf(stderr, "   w: weight-balanced tree\n");
	fprintf(stderr, "   S: skiplist\n");
	fprintf(stderr, "   H: hashtable\n");
	fprintf(stderr, "input: text file consisting of newline-separated keys"
		"\n");
	exit(EXIT_FAILURE);
    }

    srand(0xdeadbeef);

    dict_malloc_func = xmalloc;

    dict_compare_func cmp_func = my_strcmp;
    dict_hash_func hash_func = str_hash;

    dict *dct = NULL;
    const char type = argv[1][0];
    const char *container_name = NULL;
    switch (type) {
	case 'h':
	    container_name = "hb";
	    dct = hb_dict_new(cmp_func, key_str_free);
	    break;
	case 'p':
	    container_name = "pr";
	    dct = pr_dict_new(cmp_func, key_str_free);
	    break;
	case 'r':
	    container_name = "rb";
	    dct = rb_dict_new(cmp_func, key_str_free);
	    break;
	case 't':
	    container_name = "tr";
	    dct = tr_dict_new(cmp_func, NULL, key_str_free);
	    break;
	case 's':
	    container_name = "sp";
	    dct = sp_dict_new(cmp_func, key_str_free);
	    break;
	case 'S':
	    container_name = "sk";
	    dct = skiplist_dict_new(cmp_func, key_str_free, 12);
	    break;
	case 'w':
	    container_name = "wb";
	    dct = wb_dict_new(cmp_func, key_str_free);
	    break;
	case 'H':
	    container_name = "ht";
	    dct = hashtable_dict_new(cmp_func, hash_func, key_str_free, HSIZE);
	    break;
	default:
	    quit("type must be one of h, p, r, t, s, w or H");
    }

    if (!dct)
	quit("can't create container");
    ASSERT(dict_verify(dct));

    const size_t malloced_save = malloced;

    FILE *fp = fopen(argv[2], "r");
    if (fp == NULL)
	quit("cant open file '%s': %s", argv[2], strerror(errno));

    unsigned nwords = 0;
    char buf[512];
    while (fgets(buf, sizeof(buf), fp))
	++nwords;

    if (!nwords)
	quit("nothing read from file");

    char **words = xmalloc(sizeof(*words) * nwords);
    rewind(fp);
    for (unsigned i = 0; i < nwords && fgets(buf, sizeof(buf), fp); i++) {
	strtok(buf, "\n");
	words[i] = xstrdup(buf);
    }
    fclose(fp);

    malloced = malloced_save;
    size_t total_comp = 0, total_hash = 0, total_rotations = 0;

    struct rusage start, end;
    struct timeval total = { 0, 0 };

    timer_start(&start);
    for (unsigned i = 0; i < nwords; i++) {
	bool inserted = false;
	void **datum_location = dict_insert(dct, words[i], &inserted);
	if (!inserted)
	    quit("insert #%d failed for '%s'", i, words[i]);
	ASSERT(datum_location != NULL);
	ASSERT(*datum_location == NULL);
	*datum_location = words[i];
    }
    timer_end(&start, &end, &total);
    printf("    %s container: %.02fkB\n", container_name, malloced_save * 1e-3);
    printf("       %s memory: %.02fkB\n", container_name, malloced * 1e-3);
    printf("       %s insert: %.03f s (%9zu cmp, %9zu hash)\n",
	   container_name,
	   (end.ru_utime.tv_sec * 1000000 + end.ru_utime.tv_usec) * 1e-6,
	   comp_count, hash_count);
    total_comp += comp_count; comp_count = 0;
    total_hash += hash_count; hash_count = 0;
    if (type != 'H' && type != 'S') {
	tree_base *tree = dict_private(dct);
	printf("insert rotations: %zu\n", tree->rotation_count);
	total_rotations += tree->rotation_count;
	tree->rotation_count = 0;
    }

    ASSERT(dict_verify(dct));

    unsigned n = dict_count(dct);
    if (n != nwords)
	quit("bad count (%u - should be %u)!", n, nwords);

    dict_itor *itor = dict_itor_new(dct);

    timer_start(&start);
    n = 0;
    ASSERT(dict_itor_first(itor));
    do {
	ASSERT(dict_itor_valid(itor));
	ASSERT(dict_itor_key(itor) == *dict_itor_data(itor));
	++n;
    } while (dict_itor_next(itor));
    timer_end(&start, &end, &total);
    printf("  %s fwd iterate: %.03f s\n",
	   container_name,
	   (end.ru_utime.tv_sec * 1000000 + end.ru_utime.tv_usec) * 1e-6);
    if (n != nwords)
	warn("Fwd iteration returned %u items - should be %u", n, nwords);

    timer_start(&start);
    n = 0;
    ASSERT(dict_itor_last(itor));
    do {
	ASSERT(dict_itor_valid(itor));
	ASSERT(dict_itor_key(itor) == *dict_itor_data(itor));
	++n;
    } while (dict_itor_prev(itor));
    timer_end(&start, &end, &total);
    printf("  %s rev iterate: %.03f s\n",
	   container_name,
	   (end.ru_utime.tv_sec * 1000000 + end.ru_utime.tv_usec) * 1e-6);
    if (n != nwords)
	warn("Rev iteration returned %u items - should be %u", n, nwords);

    dict_itor_free(itor);

    /* shuffle(words, nwords); */

    timer_start(&start);
    for (unsigned i = 0; i < nwords; i++) {
	char *p = dict_search(dct, words[i]);
	if (!p)
	    quit("lookup failed for '%s'", buf);
	if (p != words[i])
	    quit("bad data for '%s', got '%s' instead", words[i], p);
    }
    timer_end(&start, &end, &total);
    printf("  %s good search: %.03f s (%9zu cmp, %9zu hash)\n",
	   container_name,
	   (end.ru_utime.tv_sec * 1000000 + end.ru_utime.tv_usec) * 1e-6,
	   comp_count, hash_count);
    total_comp += comp_count; comp_count = 0;
    total_hash += hash_count; hash_count = 0;
    if (type != 'H' && type != 'S') {
	tree_base *tree = dict_private(dct);
	printf("search rotations: %zu\n", tree->rotation_count);
	total_rotations += tree->rotation_count;
	tree->rotation_count = 0;
    }

    timer_start(&start);
    for (unsigned i = 0; i < nwords; i++) {
	int rv = rand() % strlen(words[i]);
	words[i][rv]++;
	dict_search(dct, words[i]);
	words[i][rv]--;
    }
    timer_end(&start, &end, &total);
    printf("   %s bad search: %.03f s (%9zu cmp, %9zu hash)\n",
	   container_name,
	   (end.ru_utime.tv_sec * 1000000 + end.ru_utime.tv_usec) * 1e-6,
	   comp_count, hash_count);
    total_comp += comp_count; comp_count = 0;
    total_hash += hash_count; hash_count = 0;

    /* shuffle(words, nwords); */

    timer_start(&start);
    for (unsigned i = 0; i < nwords; i++) {
	if (!dict_remove(dct, words[i]))
	    quit("removing #%d '%s' failed!\n", i, words[i]);
    }
    timer_end(&start, &end, &total);
    printf("       %s remove: %.03f s (%9zu cmp, %9zu hash)\n",
	   container_name,
	   (end.ru_utime.tv_sec * 1000000 + end.ru_utime.tv_usec) * 1e-6,
	   comp_count, hash_count);
    total_comp += comp_count; comp_count = 0;
    total_hash += hash_count; hash_count = 0;
    if (type != 'H' && type != 'S') {
	tree_base *tree = dict_private(dct);
	printf("remove rotations: %zu\n", tree->rotation_count);
	total_rotations += tree->rotation_count;
	tree->rotation_count = 0;
    }

    ASSERT(dict_verify(dct));

    if ((n = dict_count(dct)) != 0)
	quit("error - count not zero (%u)!", n);

    dict_free(dct);

    printf("        %s total: %.03f s (%9zu cmp, %9zu hash)\n",
	   container_name,
	   (total.tv_sec * 1000000 + total.tv_usec) * 1e-6,
	   total_comp, total_hash);

    if (type != 'H' && type != 'S') {
	printf(" total rotations: %zu\n", total_rotations);
    }

    FREE(words);

    exit(EXIT_SUCCESS);
}

char *
xstrdup(const char *str)
{
    size_t len = strlen(str) + 1;
    return memcpy(xmalloc(len), str, len);
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

void
warn(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fprintf(stderr, "warning: %s: ", appname);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void *
xmalloc(size_t size)
{
    void *p = malloc(size);
    if (!p)
	quit("out of memory");
    malloced += size;
    return p;
}

void
shuffle(char **p, unsigned size)
{
    for (unsigned i = 0; i < size - 1; i++) {
	unsigned n = rand() % (size - i);
	char *t = p[i+n]; p[i+n] = p[i]; p[i] = t;
    }
}

unsigned
str_hash(const void *p)
{
    ++hash_count;
    return dict_str_hash(p);
}

int
my_strcmp(const void *k1, const void *k2)
{
    ++comp_count;
    return strcmp(k1, k2);
}

unsigned
ptr_hash(const void *p)
{
    ++hash_count;
    return (2166136261U ^ (uintptr_t)p) * 16777619U;
}

int
my_ptrcmp(const void *k1, const void *k2)
{
    ++comp_count;
    return (k1 < k2) ? -1 : (k1 > k2);
}

void
key_str_free(void *key, void *datum)
{
    ASSERT(key == datum);
    free(key);
}

void
timer_start(struct rusage *start)
{
    getrusage(RUSAGE_SELF, start);
}

void
timer_end(const struct rusage *start, struct rusage *end,
	  struct timeval *total) {
    getrusage(RUSAGE_SELF, end);
    if (end->ru_utime.tv_usec < start->ru_utime.tv_usec)
	end->ru_utime.tv_usec += 1000000, end->ru_utime.tv_sec--;
    end->ru_utime.tv_usec -= start->ru_utime.tv_usec;
    end->ru_utime.tv_sec -= start->ru_utime.tv_sec;
    total->tv_sec += end->ru_utime.tv_sec;
    if ((total->tv_usec += end->ru_utime.tv_usec) > 1000000)
	total->tv_usec -= 1000000, total->tv_sec++;
}
