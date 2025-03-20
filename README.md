Reference Implementation for BONJSON
====================================

A C implementation to demonstrate a simple [BONJSON](https://github.com/kstenerud/bonjson/blob/main/bonjson.md) codec.

This library has no external dependencies, makes no allocations, and calls no external functions beyond `memcpy()` and some GCC intrinsics (if available).


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

Benchmarking vs [jq](https://github.com/jqlang/jq) on a Core i3-1315U (using [this test data](https://github.com/kstenerud/test-data)):

**10MB:**

```
~/ksbonjson/benchmark$ ./benchmark.sh 10mb

Benchmarking BONJSON decode+encode with 9052k file 10mb.bjn

real    0m0.021s
user    0m0.009s
sys     0m0.015s

Benchmarking JSON decode+encode with 10256k file 10mb.json

real    0m0.340s
user    0m0.322s
sys     0m0.024s
```

BONJSON processed **35.8x** faster.

**100MB:**

```
~/ksbonjson/benchmark$ ./benchmark.sh 100mb

Benchmarking BONJSON decode+encode with 90508k file 100mb.bjn

real    0m0.183s
user    0m0.085s
sys     0m0.121s

Benchmarking JSON decode+encode with 102560k file 100mb.json

real    0m3.214s
user    0m3.013s
sys     0m0.249s
```

BONJSON processed **35.4x** faster.

**1000MB:**

```
~/ksbonjson/benchmark$ ./benchmark.sh 1000mb

Benchmarking BONJSON decode+encode with 905076k file 1000mb.bjn

real    0m1.855s
user    0m0.843s
sys     0m1.229s

Benchmarking JSON decode+encode with 1025564k file 1000mb.json

real    0m32.239s
user    0m30.009s
sys     0m2.684s
```

BONJSON processed **35.6x** faster.
