/* unit_tests.c
 * Copyright (C) 2012 Farooq Mela. All rights reserved. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <float.h>
#include <time.h>

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "dict.h"

#define TEST_FUNC(func) { #func, func }

struct key_info {
    char *key, *value, *alt;
};

struct closest_lookup_info {
    const char *key;
    const char *le_key, *le_val;
    const char *lt_key, *lt_val;
    const char *ge_key, *ge_val;
    const char *gt_key, *gt_val;
};

void test_basic(dict *dct, const struct key_info *keys, const unsigned nkeys,
		const struct closest_lookup_info *cl_infos,
		unsigned n_cl_infos);
void test_basic_hashtable_1bucket();
void test_basic_hashtable2_1bucket();
void test_basic_hashtable_nbuckets();
void test_basic_hashtable2_nbuckets();
void test_basic_height_balanced_tree();
void test_basic_path_reduction_tree();
void test_basic_red_black_tree();
void test_basic_skiplist();
void test_basic_splay_tree();
void test_basic_treap();
void test_basic_weight_balanced_tree();
void test_version_string();

CU_TestInfo basic_tests[] = {
    TEST_FUNC(test_basic_hashtable_1bucket),
    TEST_FUNC(test_basic_hashtable2_1bucket),
    TEST_FUNC(test_basic_hashtable_nbuckets),
    TEST_FUNC(test_basic_hashtable2_nbuckets),
    TEST_FUNC(test_basic_height_balanced_tree),
    TEST_FUNC(test_basic_path_reduction_tree),
    TEST_FUNC(test_basic_red_black_tree),
    TEST_FUNC(test_basic_skiplist),
    TEST_FUNC(test_basic_splay_tree),
    TEST_FUNC(test_basic_treap),
    TEST_FUNC(test_basic_weight_balanced_tree),
    TEST_FUNC(test_version_string),
    CU_TEST_INFO_NULL
};

#define TEST_SUITE(suite) { .pName = #suite, .pTests = suite }

CU_SuiteInfo test_suites[] = {
    TEST_SUITE(basic_tests),
    CU_SUITE_INFO_NULL
};

int
main()
{
    CU_initialize_registry();
    CU_register_suites(test_suites);
    CU_basic_set_mode(CU_BRM_NORMAL);
    CU_basic_run_tests();
    CU_cleanup_registry();

    return 0;
}

void
shuffle(char **p, unsigned size)
{
    for (unsigned i = 0; i < size - 1; i++) {
	unsigned n = rand() % (size - i);
	char *t = p[i+n]; p[i+n] = p[i]; p[i] = t;
    }
}

static const struct key_info keys1[] = {
    { "d", "D", "d" },
    { "b", "B", "b" },
    { "a", "A", "a" },
    { "c", "C", "c" },
    { "g", "G", "g" },
    { "f", "F", "f" },
    { "h", "H", "h" },
    { "y", "Y", "y" },
    { "z", "Z", "z" },
    { "x", "X", "x" },
    { "j", "J", "j" },
    { "r", "R", "r" },
    { "q", "Q", "q" },
    { "p", "P", "p" },
    { "l", "L", "l" },
    { "m", "M", "m" },
    { "s", "S", "s" },
    { "t", "T", "t" },
    { "u", "U", "u" },
    { "da", "DA", "da" },
    { "ba", "BA", "ba" },
    { "aa", "AA", "aa" },
    { "ca", "CA", "ca" },
    { "ga", "GA", "ga" },
    { "fa", "FA", "fa" },
    { "ha", "HA", "ha" },
    { "ya", "YA", "ya" },
    { "za", "ZA", "za" },
    { "xa", "XA", "xa" },
    { "ja", "JA", "ja" },
    { "ra", "RA", "ra" },
    { "qa", "QA", "qa" },
    { "pa", "PA", "pa" },
    { "la", "LA", "la" },
    { "ma", "MA", "ma" },
    { "sa", "SA", "sa" },
    { "ta", "TA", "ta" },
    { "ua", "UA", "ua" },
};
#define NKEYS1 (sizeof(keys1) / sizeof(keys1[0]))

static const struct key_info keys2[] = {
    { "a", "A", "a" },
    { "aa", "AA", "aa" },
    { "b", "B", "b" },
    { "ba", "BA", "ba" },
    { "c", "C", "c" },
    { "ca", "CA", "ca" },
    { "d", "D", "d" },
    { "da", "DA", "da" },
    { "f", "F", "f" },
    { "fa", "FA", "fa" },
    { "g", "G", "g" },
    { "ga", "GA", "ga" },
    { "h", "H", "h" },
    { "ha", "HA", "ha" },
    { "j", "J", "j" },
    { "ja", "JA", "ja" },
    { "l", "L", "l" },
    { "la", "LA", "la" },
    { "m", "M", "m" },
    { "ma", "MA", "ma" },
    { "p", "P", "p" },
    { "pa", "PA", "pa" },
    { "q", "Q", "q" },
    { "qa", "QA", "qa" },
    { "r", "R", "r" },
    { "ra", "RA", "ra" },
    { "s", "S", "s" },
    { "sa", "SA", "sa" },
    { "t", "T", "t" },
    { "ta", "TA", "ta" },
    { "u", "U", "u" },
    { "ua", "UA", "ua" },
    { "x", "X", "x" },
    { "xa", "XA", "xa" },
    { "y", "Y", "y" },
    { "ya", "YA", "ya" },
    { "z", "Z", "z" },
    { "za", "ZA", "za" },
};
#define NKEYS2 (sizeof(keys2) / sizeof(keys2[0]))

static const struct closest_lookup_info closest_lookup_infos[] = {
    {.key = "_",
     .ge_key = "a", .ge_val = "A",
     .gt_key = "a", .gt_val = "A"},
    {.key = "a",
     .le_key = "a", .le_val = "A",
     .ge_key = "a", .ge_val = "A",
     .gt_key = "aa", .gt_val = "AA"},
    {.key = "aa",
     .le_key = "aa", .le_val = "AA",
     .lt_key = "a", .lt_val = "A",
     .ge_key = "aa", .ge_val = "AA",
     .gt_key = "b", .gt_val = "B"},
    {.key = "ab",
     .le_key = "aa", .le_val = "AA",
     .lt_key = "aa", .lt_val = "AA",
     .ge_key = "b", .ge_val = "B",
     .gt_key = "b", .gt_val = "B"},
    {.key = "m",
     .le_key = "m", .le_val = "M",
     .lt_key = "la", .lt_val = "LA",
     .ge_key = "m", .ge_val = "M",
     .gt_key = "ma", .gt_val = "MA"},
    {.key = "n",
     .le_key = "ma", .le_val = "MA",
     .lt_key = "ma", .lt_val = "MA",
     .ge_key = "p", .ge_val = "P",
     .gt_key = "p", .gt_val = "P"},
    {.key = "za",
     .le_key = "za", .le_val = "ZA",
     .lt_key = "z", .lt_val = "Z",
     .ge_key = "za", .ge_val = "ZA"},
    {.key = "zb",
     .le_key = "za", .le_val = "ZA",
     .lt_key = "za", .lt_val = "ZA"},
};

#define NUM_CLOSEST_LOOKUP_INFOS (sizeof(closest_lookup_infos) / sizeof(closest_lookup_infos[0]))

void
test_search(dict *dct, dict_itor *itor, const char *key, const char *value)
{
    CU_ASSERT_EQUAL(dict_search(dct, key), value);
    if (itor != NULL) {
	if (value == NULL) {
	    CU_ASSERT_EQUAL(dict_itor_search(itor, key), false);
	} else {
	    CU_ASSERT_EQUAL(dict_itor_search(itor, key), true);
	    CU_ASSERT_EQUAL(dict_itor_key(itor), key);
	    CU_ASSERT_EQUAL(*dict_itor_data(itor), value);
	}
    }
}

static void
test_closest_lookup(dict *dct, const struct closest_lookup_info *cl_infos, unsigned n_cl_infos)
{
    if (!dict_has_near_search(dct))
	return;

    dict_itor* itor = dict_itor_new(dct);
    for (unsigned i = 0; i < n_cl_infos; i++) {
	if (cl_infos[i].le_key) {
	    CU_ASSERT_STRING_EQUAL(dict_search_le(dct, cl_infos[i].key),
				   cl_infos[i].le_val);
	    CU_ASSERT_EQUAL(dict_itor_search_le(itor, cl_infos[i].key), true);
	    CU_ASSERT_STRING_EQUAL(dict_itor_key(itor), cl_infos[i].le_key);
	    CU_ASSERT_STRING_EQUAL(*dict_itor_data(itor), cl_infos[i].le_val);
	} else {
	    CU_ASSERT_PTR_NULL(dict_search_le(dct, cl_infos[i].key));
	    CU_ASSERT_EQUAL(dict_itor_search_le(itor, cl_infos[i].key), false);
	    CU_ASSERT_PTR_NULL(dict_itor_key(itor));
	    CU_ASSERT_PTR_NULL(dict_itor_data(itor));
	}
	if (cl_infos[i].lt_key) {
	    CU_ASSERT_STRING_EQUAL(dict_search_lt(dct, cl_infos[i].key),
				   cl_infos[i].lt_val);
	    CU_ASSERT_EQUAL(dict_itor_search_lt(itor, cl_infos[i].key), true);
	    CU_ASSERT_STRING_EQUAL(dict_itor_key(itor), cl_infos[i].lt_key);
	    CU_ASSERT_STRING_EQUAL(*dict_itor_data(itor), cl_infos[i].lt_val);
	} else {
	    CU_ASSERT_PTR_NULL(dict_search_lt(dct, cl_infos[i].key));
	    CU_ASSERT_EQUAL(dict_itor_search_lt(itor, cl_infos[i].key), false);
	    CU_ASSERT_PTR_NULL(dict_itor_key(itor));
	    CU_ASSERT_PTR_NULL(dict_itor_data(itor));
	}
	if (cl_infos[i].ge_key) {
	    CU_ASSERT_STRING_EQUAL(dict_search_ge(dct, cl_infos[i].key),
				   cl_infos[i].ge_val);
	    CU_ASSERT_EQUAL(dict_itor_search_ge(itor, cl_infos[i].key), true);
	    CU_ASSERT_STRING_EQUAL(dict_itor_key(itor), cl_infos[i].ge_key);
	    CU_ASSERT_STRING_EQUAL(*dict_itor_data(itor), cl_infos[i].ge_val);
	} else {
	    CU_ASSERT_PTR_NULL(dict_search_ge(dct, cl_infos[i].key));
	    CU_ASSERT_EQUAL(dict_itor_search_ge(itor, cl_infos[i].key), false);
	    CU_ASSERT_PTR_NULL(dict_itor_key(itor));
	    CU_ASSERT_PTR_NULL(dict_itor_data(itor));
	}
	if (cl_infos[i].gt_key) {
	    CU_ASSERT_STRING_EQUAL(dict_search_gt(dct, cl_infos[i].key),
				   cl_infos[i].gt_val);
	    CU_ASSERT_EQUAL(dict_itor_search_gt(itor, cl_infos[i].key), true);
	    CU_ASSERT_STRING_EQUAL(dict_itor_key(itor), cl_infos[i].gt_key);
	    CU_ASSERT_STRING_EQUAL(*dict_itor_data(itor), cl_infos[i].gt_val);
	} else {
	    CU_ASSERT_PTR_NULL(dict_search_gt(dct, cl_infos[i].key));
	    CU_ASSERT_EQUAL(dict_itor_search_gt(itor, cl_infos[i].key), false);
	    CU_ASSERT_PTR_NULL(dict_itor_key(itor));
	    CU_ASSERT_PTR_NULL(dict_itor_data(itor));
	}
    }
    dict_itor_free(itor);
}

void test_basic(dict *dct, const struct key_info *keys, const unsigned nkeys,
		const struct closest_lookup_info *cl_infos,
		unsigned n_cl_infos) {
    dict_itor *itor = dict_itor_new(dct);

    CU_ASSERT_TRUE(dict_verify(dct));

    for (unsigned i = 0; i < nkeys; ++i) {
	bool inserted = false;
	void **datum_location = dict_insert(dct, keys[i].key, &inserted);
	CU_ASSERT_TRUE(inserted);
	CU_ASSERT_PTR_NOT_NULL(datum_location);
	CU_ASSERT_PTR_NULL(*datum_location);
	*datum_location = keys[i].value;

	CU_ASSERT_TRUE(dict_verify(dct));

	for (unsigned j = 0; j <= i; ++j)
	    test_search(dct, itor, keys[j].key, keys[j].value);
	for (unsigned j = i + 1; j < nkeys; ++j)
	    test_search(dct, itor, keys[j].key, NULL);
    }
    CU_ASSERT_EQUAL(dict_count(dct), nkeys);

    if (dct->_vtable->insert == (dict_insert_func)hashtable_insert ||
	dct->_vtable->insert == (dict_insert_func)hashtable2_insert) {
	/* Verify that hashtable_resize works as expected. */
	dict *clone = dict_clone(dct, NULL);
	CU_ASSERT_TRUE(dict_verify(dct));
	if (dct->_vtable->insert == (dict_insert_func)hashtable_insert) {
	    CU_ASSERT_TRUE(hashtable_resize(dict_private(clone), 3));
	} else {
	    CU_ASSERT_TRUE(hashtable2_resize(dict_private(clone), 3));
	}
	CU_ASSERT_TRUE(dict_verify(dct));
	for (unsigned j = 0; j < nkeys; ++j)
	    test_search(clone, NULL, keys[j].key, keys[j].value);
	dict_free(clone);
    }

    if (dct->_vtable->clone) {
	dict *clone = dict_clone(dct, NULL);
	CU_ASSERT_PTR_NOT_NULL(clone);
	CU_ASSERT_TRUE(dict_verify(clone));
	CU_ASSERT_EQUAL(dict_count(clone), nkeys);
	for (unsigned i = 0; i < nkeys; ++i) {
	    test_search(clone, itor, keys[i].key, keys[i].value);
	}
	for (unsigned i = 0; i < nkeys; ++i) {
	    CU_ASSERT_TRUE(dict_remove(clone, keys[i].key));
	}
	dict_free(clone);
    }

    for (unsigned i = 0; i < nkeys; ++i)
	test_search(dct, itor, keys[i].key, keys[i].value);

    for (unsigned i = 0; i < nkeys; ++i) {
	bool inserted = false;
	void **datum_location = dict_insert(dct, keys[i].key, &inserted);
	CU_ASSERT_FALSE(inserted);
	CU_ASSERT_PTR_NOT_NULL(datum_location);
	CU_ASSERT_EQUAL(*datum_location, keys[i].value);

	CU_ASSERT_TRUE(dict_verify(dct));
    }
    CU_ASSERT_EQUAL(dict_count(dct), nkeys);

    CU_ASSERT_PTR_NOT_NULL(itor);
    char *last_key = NULL;
    unsigned n = 0;
    for (dict_itor_first(itor); dict_itor_valid(itor); dict_itor_next(itor)) {
	CU_ASSERT_PTR_NOT_NULL(dict_itor_key(itor));
	CU_ASSERT_PTR_NOT_NULL(dict_itor_data(itor));
	CU_ASSERT_PTR_NOT_NULL(*dict_itor_data(itor));

	char *key = dict_itor_key(itor);
	unsigned keys_matched = 0;
	for (unsigned i = 0; i < nkeys; ++i) {
	    if (keys[i].key == key) {
		CU_ASSERT_EQUAL(*dict_itor_data(itor), keys[i].value);
		keys_matched++;
	    } else {
		CU_ASSERT_NOT_EQUAL(*dict_itor_data(itor), keys[i].value);
	    }
	}
	CU_ASSERT_EQUAL(keys_matched, 1);

	if (dct->_vtable->insert != (dict_insert_func)hashtable_insert &&
	    dct->_vtable->insert != (dict_insert_func)hashtable2_insert) {
	    if (last_key) {
		CU_ASSERT_TRUE(strcmp(last_key, dict_itor_key(itor)) < 0);
	    }
	    last_key = dict_itor_key(itor);
	}

	++n;
    }
    CU_ASSERT_EQUAL(n, nkeys);
    last_key = NULL;
    n = 0;
    for (dict_itor_last(itor); dict_itor_valid(itor); dict_itor_prev(itor)) {
	CU_ASSERT_PTR_NOT_NULL(dict_itor_key(itor));
	CU_ASSERT_PTR_NOT_NULL(dict_itor_data(itor));
	CU_ASSERT_PTR_NOT_NULL(*dict_itor_data(itor));

	char *key = dict_itor_key(itor);
	unsigned keys_matched = 0;
	for (unsigned i = 0; i < nkeys; ++i) {
	    if (keys[i].key == key) {
		CU_ASSERT_EQUAL(*dict_itor_data(itor), keys[i].value);
		keys_matched++;
	    } else {
		CU_ASSERT_NOT_EQUAL(*dict_itor_data(itor), keys[i].value);
	    }
	}
	CU_ASSERT_EQUAL(keys_matched, 1);

	if (dct->_vtable->insert != (dict_insert_func)hashtable_insert &&
	    dct->_vtable->insert != (dict_insert_func)hashtable2_insert) {
	    if (last_key) {
		CU_ASSERT_TRUE(strcmp(last_key, dict_itor_key(itor)) > 0);
	    }
	    last_key = dict_itor_key(itor);
	}

	++n;
    }
    CU_ASSERT_EQUAL(n, nkeys);

    for (unsigned i = 0; i < nkeys; ++i) {
	bool inserted = false;
	void **datum_location = dict_insert(dct, keys[i].key, &inserted);
	CU_ASSERT_FALSE(inserted);
	CU_ASSERT_PTR_NOT_NULL(datum_location);
	CU_ASSERT_PTR_NOT_NULL(*datum_location);
	*datum_location = keys[i].alt;

	CU_ASSERT_TRUE(dict_verify(dct));
    }
    CU_ASSERT_EQUAL(dict_count(dct), nkeys);

    for (unsigned i = 0; i < nkeys; ++i)
	test_search(dct, itor, keys[i].key, keys[i].alt);

    for (unsigned i = 0; i < nkeys; ++i) {
	test_search(dct, itor, keys[i].key, keys[i].alt);
	CU_ASSERT_TRUE(dict_remove(dct, keys[i].key));
	CU_ASSERT_TRUE(dict_verify(dct));

	CU_ASSERT_EQUAL(dict_remove(dct, keys[i].key), false);
	for (unsigned j = 0; j <= i; ++j) {
	    test_search(dct, itor, keys[j].key, NULL);
	}
	for (unsigned j = i + 1; j < nkeys; ++j) {
	    test_search(dct, itor, keys[j].key, keys[j].alt);
	}
    }

    for (unsigned i = 0; i < nkeys; ++i) {
	bool inserted = false;
	void **datum_location = dict_insert(dct, keys[i].key, &inserted);
	CU_ASSERT_TRUE(inserted);
	CU_ASSERT_PTR_NOT_NULL(datum_location);
	CU_ASSERT_PTR_NULL(*datum_location);
	*datum_location = keys[i].value;

	CU_ASSERT_TRUE(dict_verify(dct));
    }
    CU_ASSERT_EQUAL(dict_count(dct), nkeys);
    CU_ASSERT_EQUAL(dict_clear(dct), nkeys);

    for (unsigned i = 0; i < nkeys; ++i) {
	bool inserted = false;
	void **datum_location = dict_insert(dct, keys[i].key, &inserted);
	CU_ASSERT_TRUE(inserted);
	CU_ASSERT_PTR_NOT_NULL(datum_location);
	CU_ASSERT_PTR_NULL(*datum_location);
	*datum_location = keys[i].value;

	CU_ASSERT_TRUE(dict_verify(dct));
    }
    test_closest_lookup(dct, cl_infos, n_cl_infos);
    dict_itor_free(itor);
    CU_ASSERT_EQUAL(dict_count(dct), nkeys);
    CU_ASSERT_EQUAL(dict_free(dct), nkeys);
}

unsigned
strhash(const void *p)
{
    unsigned hash = 2166136261U;
    for (const uint8_t *ptr = p; *ptr;) {
	hash = (hash ^ *ptr++) * 16777619U;
    }
    return hash;
}

void test_basic_hashtable_1bucket()
{
    test_basic(hashtable_dict_new(dict_str_cmp, strhash, NULL, 1),
	       keys1, NKEYS1, closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
    test_basic(hashtable_dict_new(dict_str_cmp, strhash, NULL, 1),
	       keys2, NKEYS2, closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
}

void test_basic_hashtable2_1bucket()
{
    test_basic(hashtable2_dict_new(dict_str_cmp, strhash, NULL, 1),
	       keys1, NKEYS1, closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
    test_basic(hashtable2_dict_new(dict_str_cmp, strhash, NULL, 1),
	       keys2, NKEYS2, closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
}

void test_basic_hashtable_nbuckets()
{
    test_basic(hashtable_dict_new(dict_str_cmp, strhash, NULL, 7),
	       keys1, NKEYS1, closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
    test_basic(hashtable_dict_new(dict_str_cmp, strhash, NULL, 7),
	       keys2, NKEYS2, closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
}

void test_basic_hashtable2_nbuckets()
{
    test_basic(hashtable2_dict_new(dict_str_cmp, strhash, NULL, 7),
	       keys1, NKEYS1, closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
    test_basic(hashtable2_dict_new(dict_str_cmp, strhash, NULL, 7),
	       keys2, NKEYS2, closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
}

void test_basic_height_balanced_tree()
{
    test_basic(hb_dict_new(dict_str_cmp, NULL), keys1, NKEYS1,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
    test_basic(hb_dict_new(dict_str_cmp, NULL), keys2, NKEYS2,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
}

void test_basic_path_reduction_tree()
{
    test_basic(pr_dict_new(dict_str_cmp, NULL), keys1, NKEYS1,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
    test_basic(pr_dict_new(dict_str_cmp, NULL), keys2, NKEYS2,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
}

void test_basic_red_black_tree()
{
    test_basic(rb_dict_new(dict_str_cmp, NULL), keys1, NKEYS1,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
    test_basic(rb_dict_new(dict_str_cmp, NULL), keys2, NKEYS2,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
}

void test_basic_skiplist()
{
    test_basic(skiplist_dict_new(dict_str_cmp, NULL, 13), keys1, NKEYS1,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
    test_basic(skiplist_dict_new(dict_str_cmp, NULL, 13), keys2, NKEYS2,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
}

void test_basic_splay_tree()
{
    test_basic(sp_dict_new(dict_str_cmp, NULL), keys1, NKEYS1,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
    test_basic(sp_dict_new(dict_str_cmp, NULL), keys2, NKEYS2,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
}

void test_basic_treap()
{
    test_basic(tr_dict_new(dict_str_cmp, NULL, NULL), keys1, NKEYS1,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
    test_basic(tr_dict_new(dict_str_cmp, NULL, NULL), keys2, NKEYS2,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
}

void test_basic_weight_balanced_tree()
{
    test_basic(wb_dict_new(dict_str_cmp, NULL), keys1, NKEYS1,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
    test_basic(wb_dict_new(dict_str_cmp, NULL), keys2, NKEYS2,
	       closest_lookup_infos, NUM_CLOSEST_LOOKUP_INFOS);
}

void test_version_string()
{
    char version_string[32];
    snprintf(version_string, sizeof(version_string), "%d.%d.%d",
	     DICT_VERSION_MAJOR, DICT_VERSION_MINOR, DICT_VERSION_PATCH);
    CU_ASSERT_STRING_EQUAL(kDictVersionString, version_string);
}
