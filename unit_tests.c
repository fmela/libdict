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

void test_basic();

CU_TestInfo basic_tests[] = {
    TEST_FUNC(test_basic),
    CU_TEST_INFO_NULL
};

#define TEST_SUITE(suite) { #suite, NULL, NULL, suite }

CU_SuiteInfo test_suites[] = {
    TEST_SUITE(basic_tests),
    CU_SUITE_INFO_NULL
};

int
main(void)
{
    CU_initialize_registry();
    CU_register_suites(test_suites);
    CU_basic_set_mode(CU_BRM_NORMAL);
    CU_basic_run_tests();
    CU_cleanup_registry();

    return 0;
}

void test_basic()
{
}
