project(
  'libdict',
  'c',
  default_options: ['c_std=c11'],
  version: '0.1.0',
  license: 'BSD2')

incdir = include_directories('include', 'src')
libdict = both_libraries(
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
  include_directories: incdir,
  install: true)

cunit = dependency('cunit')

executable('demo', 'demo.c', link_with: libdict, include_directories: incdir)
executable('benchmark_bin', 'benchmark.c', link_with: libdict, include_directories: incdir)
executable('unit_tests', 'unit_tests.c', link_with: libdict, include_directories: incdir, dependencies: cunit)

install_headers(
  'include/dict.h',
  'include/hashtable.h',
  'include/hashtable2.h',
  'include/hb_tree.h',
  'include/pr_tree.h',
  'include/rb_tree.h',
  'include/sp_tree.h',
  'include/tr_tree.h',
  'include/wb_tree.h',
  'include/skiplist.h',
  subdir: 'libdict')

pkg = import('pkgconfig')
pkg.generate(
  libdict,
  description: 'Data structures with efficient insert, lookup and delete routines.')
