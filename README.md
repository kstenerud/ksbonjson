Reference Implementation for BONJSON
====================================

A C implementation to demonstrate a simple [BONJSON](https://github.com/kstenerud/bonjson/blob/main/bonjson.md) codec.

This library has no external dependencies, makes no allocations, and calls no external functions beyond `memcpy()` and `memchr()`.


Build Requirements
------------------

* Meson 0.49 or newer
* Ninja 1.8.2 or newer
* A C compiler
* A C++ compiler (for the tests)


Points of Interest
------------------

* [Library](library)
* [JSON <-> BONJSON command-line program](executable)
* [Benchmarking program](benchmark)


Last Benchmark Run
------------------

Benchmarking vs [jq](https://github.com/jqlang/jq) using [this test data](https://github.com/kstenerud/test-data):

```
~/ksbonjson/benchmark$ ./benchmark.sh 1000mb

Benchmarking BONJSON decode+encode with 902208k file 1000mb.bjn

real    0m1.849s
user    0m0.889s
sys     0m1.152s

Benchmarking JSON decode+encode with 1025564k file 1000mb.json

real    0m32.455s
user    0m30.295s
sys     0m2.693s
```
