Benchmarking Executable for BONJSON
===================================

Converts BONJSON to BONJSON, reading input from stdin and writing to stdout.


Requirements
------------

* Meson 0.49 or newer
* Ninja 1.8.2 or newer
* A C compiler


Building
--------

    meson build
    ninja -C build


Usage
-----

    Usage: bonjson-benchmark -i some-data.bjn -o result.bjn
