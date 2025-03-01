Reference Implementation for BONJSON
====================================

A C implementation to demonstrate a simple [BONJSON](https://github.com/kstenerud/bonjson/blob/main/bonjson.md) codec.

This library has no external dependencies, makes no allocations, and calls no external functions beyond `memcpy()`.


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

(using jq for JSON benchmark)

    ~/ksbonjson/benchmark$ ./benchmark.sh large-file

    Benchmarking BONJSON decode+encode with 23364k file large-file.bjn

    real    0m0.086s
    user    0m0.060s
    sys     0m0.034s

    Benchmarking JSON decode+encode with 25532k file large-file.json

    real    0m0.821s
    user    0m0.763s
    sys     0m0.070s
