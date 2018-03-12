# libdict

[![Build Status](https://travis-ci.org/fmela/libdict.svg?branch=master)](https://travis-ci.org/fmela/libdict)

libdict is a C library that provides the following data structures with efficient insert, lookup, and delete routines:

* [height-balanced (AVL) tree](http://en.wikipedia.org/wiki/AVL_tree)
* [red-black tree](http://en.wikipedia.org/wiki/Red-black_tree)
* [splay tree](http://en.wikipedia.org/wiki/Splay_tree)
* [weight-balanced tree](https://en.wikipedia.org/wiki/Weight-balanced_tree)
* [path-reduction tree](https://cs.uwaterloo.ca/research/tr/1982/CS-82-07.pdf)
* [treap](http://en.wikipedia.org/wiki/Treap)
* [hashtable using separate chaining](http://en.wikipedia.org/wiki/Hashtable#Separate_chaining)
* [hashtable using open addressing with linear probing](http://en.wikipedia.org/wiki/Hashtable#Open_addressing)
* [skip list](https://en.wikipedia.org/wiki/Skip_list)

All data structures in this library support insert, search, and remove, and have bidirectional iterators. The sorted data structures (everything but hash tables) support near-search operations: searching for the key greater or equal to, strictly greater than, lesser or equal to, or strictly less than, a given key. The tree data structures also support the selecting the nth element; this takes linear time, except in path-reduction and weight-balanced trees, where it only takes logarithmic time.

The API and code are written with efficiency as a primary concern. For example, an insert call returns a boolean indicating whether or not the key was already present in the dictionary (i.e. whether there was an insertion or a collision), and a pointer to the location of the associated data. Thus, an insert-or-update operation can be supported with a single traversal of the data structure. In addition, almost all recursive algorithms have been rewritten to use iteration instead.

## License

libdict is released under the simplified BSD [license](LICENSE).
