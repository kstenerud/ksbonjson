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

Benchmarking BONJSON (decode+encode) with 9052k file 10mb.boj

real    0m0.021s
user    0m0.009s
sys     0m0.016s

Benchmarking BONJSON (decode only) with 9052k file 10mb.boj

real    0m0.011s
user    0m0.003s
sys     0m0.012s

Benchmarking JSON (decode+encode) with 10256k file 10mb.json

real    0m0.340s
user    0m0.322s
sys     0m0.024s
```

BONJSON processed **35.8x** faster.

**100MB:**

```
~/ksbonjson/benchmark$ ./benchmark.sh 100mb

Benchmarking BONJSON (decode+encode) with 90508k file 100mb.boj

real    0m0.183s
user    0m0.085s
sys     0m0.121s

Benchmarking BONJSON (decode only) with 90508k file 100mb.boj

real    0m0.080s
user    0m0.030s
sys     0m0.071s

Benchmarking JSON (decode+encode) with 102560k file 100mb.json

real    0m3.227s
user    0m3.039s
sys     0m0.232s
```

BONJSON processed **35.8x** faster.

**1000MB:**

```
~/ksbonjson/benchmark$ ./benchmark.sh 1000mb

Benchmarking BONJSON (decode+encode) with 905076k file 1000mb.boj

real    0m1.762s
user    0m0.846s
sys     0m1.124s

Benchmarking BONJSON (decode only) with 905076k file 1000mb.boj

real    0m0.746s
user    0m0.300s
sys     0m0.645s

Benchmarking JSON (decode+encode) with 1025564k file 1000mb.json

real    0m31.522s
user    0m29.737s
sys     0m2.307s
```

BONJSON processed **35.2x** faster.
