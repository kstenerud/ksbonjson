Reference Implementation for BONJSON
====================================

A C implementation to demonstrate a simple [BONJSON](https://github.com/kstenerud/bonjson/blob/main/bonjson.md) codec.

The library has no external dependencies, makes no allocations, and calls no external functions (not even from libc).


### Build Requirements

  * Meson 0.49 or newer
  * Ninja 1.8.2 or newer
  * A C compiler
  * A C++ compiler (for the tests)


### Points of Interest

* [Library](library)
* [JSON <-> BONJSON command-line program](executable)
