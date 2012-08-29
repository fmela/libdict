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

void test_basic(dict *dct);
void test_basic_hashtable_1();
void test_basic_hashtable_n();
void test_basic_hb();
void test_basic_pr();
void test_basic_rb();
void test_basic_sp();
void test_basic_tr();
void test_basic_wb();

CU_TestInfo basic_tests[] = {
    TEST_FUNC(test_basic_hashtable_1),
    TEST_FUNC(test_basic_hashtable_n),
    TEST_FUNC(test_basic_hb),
    TEST_FUNC(test_basic_pr),
    TEST_FUNC(test_basic_rb),
    TEST_FUNC(test_basic_sp),
    TEST_FUNC(test_basic_tr),
    TEST_FUNC(test_basic_wb),
    CU_TEST_INFO_NULL
};

#define TEST_SUITE(suite) { #suite, NULL, NULL, suite }

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

static struct {
    char *key;
    char *value;
    char *alt;
} keys[] = {
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
};
#define NKEYS (sizeof(keys) / sizeof(keys[0]))

void test_basic(dict *dct) {
    for (unsigned i = 0; i < NKEYS; ++i)
	CU_ASSERT_EQUAL(dict_insert(dct, keys[i].key, keys[i].value, false), 0);
    CU_ASSERT_EQUAL(dict_count(dct), NKEYS);

    for (unsigned i = 0; i < NKEYS; ++i)
	CU_ASSERT_EQUAL(dict_search(dct, keys[i].key), keys[i].value);

    for (unsigned i = 0; i < NKEYS; ++i)
	CU_ASSERT_EQUAL(dict_insert(dct, keys[i].key, keys[i].alt, false), 1);
    CU_ASSERT_EQUAL(dict_count(dct), NKEYS);

    for (unsigned i = 0; i < NKEYS; ++i)
	CU_ASSERT_EQUAL(dict_insert(dct, keys[i].key, keys[i].alt, true), 0);
    CU_ASSERT_EQUAL(dict_count(dct), NKEYS);

    for (unsigned i = 0; i < NKEYS; ++i)
	CU_ASSERT_EQUAL(dict_search(dct, keys[i].key), keys[i].alt);

    for (unsigned i = 0; i < NKEYS; ++i) {
	CU_ASSERT_EQUAL(dict_search(dct, keys[i].key), keys[i].alt);
	CU_ASSERT_EQUAL(dict_remove(dct, keys[i].key), true);
	CU_ASSERT_EQUAL(dict_search(dct, keys[i].key), NULL);
	CU_ASSERT_EQUAL(dict_remove(dct, keys[i].key), false);
	for (unsigned j = i + 1; j < NKEYS; ++j) {
	    CU_ASSERT_EQUAL(dict_search(dct, keys[j].key), keys[j].alt);
	}
	CU_ASSERT_EQUAL(dict_remove(dct, keys[i].key), false);
    }

    CU_ASSERT_EQUAL(dict_clear(dct), 0);
    CU_ASSERT_EQUAL(dict_count(dct), 0);
    dict_free(dct);
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
void test_basic_hashtable_1()
{
    test_basic(hashtable_dict_new(dict_str_cmp, strhash, NULL, 1));
}

void test_basic_hashtable_n()
{
    test_basic(hashtable_dict_new(dict_str_cmp, strhash, NULL, 37));
}

void test_basic_hb()
{
    test_basic(hb_dict_new(dict_str_cmp, NULL));
}

void test_basic_pr()
{
    test_basic(pr_dict_new(dict_str_cmp, NULL));
}

void test_basic_rb()
{
    test_basic(rb_dict_new(dict_str_cmp, NULL));
}

void test_basic_sp()
{
    test_basic(sp_dict_new(dict_str_cmp, NULL));
}

void test_basic_tr()
{
    test_basic(tr_dict_new(dict_str_cmp, NULL, NULL));
}

void test_basic_wb()
{
    test_basic(wb_dict_new(dict_str_cmp, NULL));
}
