/* demo.c
 * Interactive demo of libdict.
 * Copyright (C) 2001-2011 Farooq Mela */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include "dict.h"

const char appname[] = "demo";

char *xstrdup(const char *str);

#ifdef __GNUC__
# define NORETURN	__attribute__((__noreturn__))
#else
# define NORETURN
#endif
void quit(const char *, ...) NORETURN;
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
void *xdup(const void *ptr, size_t size);

void
key_val_free(void *key, void *datum)
{
    free(key);
    free(datum);
}

#define HSIZE		997

int
main(int argc, char **argv)
{
    char buf[512], *p, *ptr, *ptr2;
    int rv;
    dict *dct;

    if (argc != 2)
	quit("usage: %s [type]", appname);

    srand((unsigned)time(NULL));

    dict_malloc_func = xmalloc;

    ++argv;
    switch (argv[0][0]) {
	case 'h':
	    dct = hb_dict_new((dict_compare_func)strcmp, key_val_free);
	    break;
	case 'p':
	    dct = pr_dict_new((dict_compare_func)strcmp, key_val_free);
	    break;
	case 'r':
	    dct = rb_dict_new((dict_compare_func)strcmp, key_val_free);
	    break;
	case 't':
	    dct = tr_dict_new((dict_compare_func)strcmp, NULL, key_val_free);
	    break;
	case 's':
	    dct = sp_dict_new((dict_compare_func)strcmp, key_val_free);
	    break;
	case 'w':
	    dct = wb_dict_new((dict_compare_func)strcmp, key_val_free);
	    break;
	case 'H':
	    dct = hashtable_dict_new((dict_compare_func)strcmp,
				     dict_str_hash,
				     key_val_free, HSIZE);
	    break;
	default:
	    quit("type must be one of h, p, r, t, s, w, or H");
    }

    if (!dct)
	quit("can't create container");

    for (;;) {
	printf("> ");
	fflush(stdout);
	if (fgets(buf, sizeof(buf), stdin) == NULL)
	    break;
	if ((p = strchr(buf, '\n')) != NULL)
	    *p = 0;
	for (p = buf; isspace(*p); p++)
	    /* void */;
	strcpy(buf, p);
	ptr2 = (ptr = strtok(buf, " ") ? strtok(NULL, " ") : NULL) ?
	    strtok(NULL, " ") : NULL;
	if (*buf == 0)
	    continue;
	if (strcmp(buf, "insert") == 0) {
	    if (!ptr2) {
		printf("usage: insert <key> <data>\n");
		continue;
	    }
	    void **datum_location;
	    if (dict_insert(dct, xstrdup(ptr), &datum_location)) {
		*datum_location = xstrdup(ptr2);
		printf("inserted '%s': '%s'\n",
		       ptr, *datum_location);
	    } else {
		printf("key '%s' already in dict: '%s'\n",
		       ptr, *datum_location);
	    }
	} else if (strcmp(buf, "search") == 0) {
	    if (ptr2) {
		printf("usage: search <key>\n");
		continue;
	    }
	    ptr2 = dict_search(dct, ptr);
	    if (ptr2)
		printf("found '%s': '%s'\n", ptr, ptr2);
	    else
		printf("key '%s' not in dict!\n", ptr);
	} else if (strcmp(buf, "remove") == 0) {
	    if (!ptr || ptr2) {
		printf("usage: remove <key>\n");
		continue;
	    }
	    rv = dict_remove(dct, ptr);
	    if (rv == 0)
		printf("removed '%s' from dict\n", ptr);
	    else
		printf("key '%s' not in dict!\n", ptr);
	} else if (strcmp(buf, "show") == 0) {
	    if (ptr) {
		printf("usage: show\n");
		continue;
	    }
	    dict_itor *itor = dict_itor_new(dct);
	    dict_itor_first(itor);
	    for (; dict_itor_valid(itor); dict_itor_next(itor))
		printf("'%s': '%s'\n",
		       (char *)dict_itor_key(itor),
		       (char *)dict_itor_data(itor));
	    dict_itor_free(itor);
	} else if (strcmp(buf, "reverse") == 0) {
	    if (ptr) {
		printf("usage: reverse\n");
		continue;
	    }
	    dict_itor *itor = dict_itor_new(dct);
	    dict_itor_last(itor);
	    for (; dict_itor_valid(itor); dict_itor_prev(itor))
		printf("'%s': '%s'\n",
		       (char *)dict_itor_key(itor),
		       (char *)dict_itor_data(itor));
	    dict_itor_free(itor);
	} else if (strcmp(buf, "clear") == 0) {
	    if (ptr) {
		printf("usage: clear\n");
		continue;
	    }
	    dict_clear(dct);
	} else if (strcmp(buf, "count") == 0) {
	    if (ptr) {
		printf("usage: count\n");
		continue;
	    }
	    printf("count = %zu\n", dict_count(dct));
	} else if (strcmp(buf, "quit") == 0) {
	    break;
	} else {
	    printf("Usage summary:\n");
	    printf("  insert <key> <data>\n");
	    printf("  search <key>\n");
	    printf("  remove <key>\n");
	    printf("  clear\n");
	    printf("  count\n");
	    printf("  show\n");
	    printf("  reverse\n");
	    printf("  quit\n");
	}
    }

    dict_free(dct);

    exit(0);
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
    void *p = malloc(size);
    if (!p) {
	fprintf(stderr, "out of memory\n");
	abort();
    }
    return p;
}

void *
xdup(const void *ptr, size_t size)
{
    return memcpy(xmalloc(size), ptr, size);
}
