cmake_minimum_required(VERSION 2.8)
set(_ECLIPSE_VERSION 4.5)
project(libdict)

enable_language(C)
add_definitions(-std=c11)

set(LIBDICT_SOURCES
    src/dict.c
    src/hashtable.c
    src/hashtable2.c
    src/hashtable_common.c
    src/hb_tree.c
    src/pr_tree.c
    src/rb_tree.c
    src/skiplist.c
    src/sp_tree.c
    src/tr_tree.c
    src/tree_common.c
    src/wb_tree.c
)

include_directories(include)
include_directories(src)

add_library(dict STATIC ${LIBDICT_SOURCES})
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
    set_target_properties(dict PROPERTIES COMPILE_FLAGS "-fPIC")
endif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
set_target_properties(dict PROPERTIES PREFIX "lib")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")
find_package(CUnit REQUIRED)
enable_testing()
add_executable(dict_test unit_tests.c)
target_include_directories(dict_test PRIVATE ${CUNIT_INCLUDE_DIRS})
target_link_libraries (dict_test dict)
target_link_libraries(dict_test ${CUNIT_LIBRARIES})

add_test(
    NAME dict_test
    COMMAND dict_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

