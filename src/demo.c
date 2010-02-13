/* demo.c
 * Demo capabilities of libdict
 * Copyright (C) 2001-2004 Farooq Mela <fmela@uci.edu> */

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
void *xcalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
void *xdup(const void *ptr, size_t size);

unsigned s_hash(const unsigned char *p);

unsigned
s_hash(const unsigned char *p)
{
	unsigned hash = 0;

	while (*p) {
		hash *= 31;
		hash ^= *p++;
	}
	return hash;
}

#define HSIZE		30011

int
main(int argc, char **argv)
{
	char buf[512], *p, *ptr, *ptr2;
	int rv;
	dict *dct;

	if (argc != 2)
		quit("usage: %s [type]", appname);

	srand((unsigned)time(NULL));

	dict_set_malloc(xmalloc);

	++argv;
	switch (argv[0][0]) {
	case 'h':
		dct = hb_dict_new((dict_cmp_func)strcmp, free, free);
		break;
	case 'p':
		dct = pr_dict_new((dict_cmp_func)strcmp, free, free);
		break;
	case 'r':
		dct = rb_dict_new((dict_cmp_func)strcmp, free, free);
		break;
	case 't':
		dct = tr_dict_new((dict_cmp_func)strcmp, free, free);
		break;
	case 's':
		dct = sp_dict_new((dict_cmp_func)strcmp, free, free);
		break;
	case 'w':
		dct = wb_dict_new((dict_cmp_func)strcmp, free, free);
		break;
	case 'H':
		dct = hashtable_dict_new((dict_cmp_func)strcmp, (dict_hsh_func)s_hash,
								 free, free, HSIZE);
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
			rv = dict_insert(dct, xstrdup(ptr), xstrdup(ptr2), FALSE);
			if (rv == 0)
				printf("inserted `%s' ==> `%s'\n", ptr, ptr2);
			else
				printf("key `%s' already in dict!\n", ptr);
		} else if (strcmp(buf, "search") == 0) {
			if (ptr2) {
				printf("usage: search <key>\n");
				continue;
			}
			ptr2 = dict_search(dct, ptr);
			if (ptr2)
				printf("found `%s' ==> `%s'\n", ptr, ptr2);
			else
				printf("key `%s' not in dict!\n", ptr);
		} else if (strcmp(buf, "remove") == 0) {
			if (!ptr || ptr2) {
				printf("usage: remove <key>\n");
				continue;
			}
			rv = dict_remove(dct, ptr, TRUE);
			if (rv == 0)
				printf("removed `%s' from dict\n", ptr);
			else
				printf("key `%s' not in dict!\n", ptr);
		} else if (strcmp(buf, "display") == 0) {
			dict_itor *itor;

			if (ptr) {
				printf("usage: display\n");
				continue;
			}
			itor = dict_itor_new(dct);
			for (; dict_itor_valid(itor); dict_itor_next(itor))
				printf("`%s' ==> `%s'\n",
					   (char *)dict_itor_key(itor),
					   (char *)dict_itor_data(itor));
			dict_itor_destroy(itor);
		} else if (strcmp(buf, "empty") == 0) {
			if (ptr) {
				printf("usage: empty\n");
				continue;
			}
			dict_empty(dct, TRUE);
		} else if (strcmp(buf, "count") == 0) {
			if (ptr) {
				printf("usage: count\n");
				continue;
			}
			printf("count = %u\n", dict_count(dct));
		} else if (strcmp(buf, "quit") == 0) {
			break;
		} else {
			printf("Usage summary:\n");
			printf("  insert <key> <data>\n");
			printf("  search <key>\n");
			printf("  remove <key>\n");
			printf("  empty\n");
			printf("  count\n");
			printf("  display\n");
			printf("  quit\n");
		}
	}

	dict_destroy(dct, TRUE);

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
	void *p;

	if ((p = malloc(size)) == NULL)
		quit("out of memory");
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
