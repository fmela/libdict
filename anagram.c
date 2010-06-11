#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "dict.h"

void *xmalloc(size_t size);
char *xstrdup(const char *s);

struct words {
	char *word;
	struct words *next;
};

int
main(int argc, char *argv[])
{
	int i, freq[256];
	char buf[512], name[1024], *key;
	FILE *fp;
	hb_tree *tree;
	hb_itor *itor;
	struct words *word, *wordp;

	if (argv[1] == NULL) {
		printf("Expected filename argument.\n");
		exit(1);
	}

	if ((fp = fopen(argv[1], "r")) == NULL) {
		printf("Unable to open file '%s'.\n", argv[1]);
		exit(1);
	}

	tree = hb_tree_new(dict_str_cmp, NULL);

	while (fgets(buf, sizeof(buf), fp)) {
		char *p;

		if (isupper(buf[0]))	/* Disregard proper nouns. */
			continue;

		strtok(buf, "\r\n");
		memset(freq, 0, sizeof(freq));

		assert(buf[0] != '\0');

		for (p = buf; *p; p++)
			freq[tolower(*p)]++;

		p = name;
		for (i=1; i<256; i++) {
			if (freq[i]) {
				assert(freq[i] < 10);

				*p++ = i;
				*p++ = '0' + freq[i];
			}
		}
		*p = 0;

		key = xstrdup(name);

		word = xmalloc(sizeof(*word));
		word->word = xstrdup(buf);
		word->next = NULL;

		wordp = word;
		if (hb_tree_probe(tree, key, (void **)&wordp) == 0) {
			/* Key already exists. */
			word->next = wordp->next;
			wordp->next = word;
		}
	}

	itor = hb_itor_new(tree);
	do {
		word = hb_itor_data(itor);
		if (word->next) {
			int count = 1;
			while (word->next)
				count++, word = word->next;
			printf("%2d:[", count);
			word = hb_itor_data(itor);
			while (word) {
				printf("%s%c", word->word, word->next ? ',' : ']');
				word = word->next;
			}
			printf("\n");
		}
	} while (hb_itor_next(itor));

	return 0;
}

char *
xstrdup(const char *s)
{
	size_t len = strlen(s) + 1;
	return memcpy(xmalloc(len), s, len + 1);
}

void *
xmalloc(size_t size)
{
	void *p;

	assert(size > 0);

	p = malloc(size);
	if (p == NULL) {
		printf("Out of memory\n");
		abort();
	}
	return p;
}
