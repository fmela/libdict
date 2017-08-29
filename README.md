# libdict

libdict is a C library that provides the following data structures with efficient insert, lookup, and delete routines:

* [height-balanced (AVL) tree](http://en.wikipedia.org/wiki/AVL_tree)
* [red-black tree](http://en.wikipedia.org/wiki/Red-black_tree)
* [splay tree](http://en.wikipedia.org/wiki/Splay_tree)
* [weight-balanced tree](https://en.wikipedia.org/wiki/Weight-balanced_tree)
* [path-reduction tree](https://cs.uwaterloo.ca/research/tr/1982/CS-82-07.pdf)
* [treap](http://en.wikipedia.org/wiki/Treap)
* [hashtable, using separate chaining](http://en.wikipedia.org/wiki/Hashtable#Separate_chaining)
* [hashtable, using open addressing with linear probing](http://en.wikipedia.org/wiki/Hashtable#Open_addressing)

The API is designed with efficiency as its primary concern. For example, an insert call returns a boolean indicating whether or not the key was already present in the dictionary (i.e. whether there was an insertion or a collision), and a pointer to the location of the associated data. Thus, an insert-or-update operation can be supported with a single traversal of the data structure.

All data structures support bidirectional iterators.

## Build status

[![Build Status](https://travis-ci.org/fmela/libdict.svg?branch=master)](https://travis-ci.org/fmela/libdict)

## License

libdict is released under the simplified BSD [license](LICENSE).
