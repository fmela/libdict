# libdict

libdict is a C library that provides the following data structures with efficient insert, lookup, and delete routines:

* [height-balanced (AVL) tree](http://en.wikipedia.org/wiki/AVL_tree)
* [red-black tree](http://en.wikipedia.org/wiki/Red-black_tree)
* [splay tree](http://en.wikipedia.org/wiki/Splay_tree)
* [weight-balanced tree](https://en.wikipedia.org/wiki/Weight-balanced_tree)
* [path-reduction tree](https://cs.uwaterloo.ca/research/tr/1982/CS-82-07.pdf)
* [treap](http://en.wikipedia.org/wiki/Treap)
* [hashtable using separate chaining](http://en.wikipedia.org/wiki/Hashtable#Separate_chaining)
* [hashtable using open addressing with linear probing](http://en.wikipedia.org/wiki/Hashtable#Open_addressing)

All data structures in this library support insert, search, and remove, and have bidirectional iterators. In addition, the sorted data structures (i.e. everything except hash tables) support near-search operations: searching for the element greater or equal to, strictly greater than, lesser or equal to, or strictly less than, a given key. The sorted data structures also support the selecting the nth element, which takes linear time generally, but only takes logarithmic time in weight-balanced trees.

The API is designed with efficiency as a primary concern. For example, an insert call returns a boolean indicating whether or not the key was already present in the dictionary (i.e. whether there was an insertion or a collision), and a pointer to the location of the associated data. Thus, an insert-or-update operation can be supported with a single traversal of the data structure. In addition, the code is written to be very efficient, and almost all recursive algorithms have been rewritten to use iteration instead.

## Build status

[![Build Status](https://travis-ci.org/fmela/libdict.svg?branch=master)](https://travis-ci.org/fmela/libdict)

## License

libdict is released under the simplified BSD [license](LICENSE).
