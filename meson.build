project(
  'libdict',
  'c',
  default_options: ['c_std=c11'],
  version: '0.1.0',
  license: 'BSD2')

incdir = include_directories('include', 'src')
libdict = library(
  'dict',
  'src/dict.c',
  'src/hashtable.c',
  'src/hashtable2.c',
  'src/hashtable_common.c',
  'src/hb_tree.c',
  'src/pr_tree.c',
  'src/rb_tree.c',
  'src/sp_tree.c',
  'src/tr_tree.c',
  'src/wb_tree.c',
  'src/skiplist.c',
  'src/tree_common.c',
  include_directories: incdir)

cunit = dependency('cunit')

executable('demo', 'demo.c', link_with: libdict, include_directories: incdir)
executable('benchmark_bin', 'benchmark.c', link_with: libdict, include_directories: incdir)
executable('unit_tests', 'unit_tests.c', link_with: libdict, include_directories: incdir, dependencies: cunit)
