Reference Executable for BONJSON
================================

A C implementation to demonstrate a simple [BONJSON](https://github.com/kstenerud/bonjson/blob/main/bonjson.md) codec.

Converts between [JSON](https://www.json.org) and [BONJSON](https://github.com/kstenerud/bonjson/blob/main/bonjson.md).

JSON functionality is provided by [json-c](https://github.com/json-c/json-c).


Requirements
------------

  * Meson 0.49 or newer
  * Ninja 1.8.2 or newer
  * CMake (for json-c)
  * A C compiler


Building
--------

    meson build
    ninja -C build


Usage
-----

    Usage: bonjson [options]
    Where the default behavior is to convert from stdin to stdout.

    Options:
      -h: Print help and exit
      -v: Print version and exit
      -i <path>: Input file (use - to specify stdin) (default stdin)
      -o <path>: Output file (use - to specify stdout) (default stdout)
      -b: Convert JSON to BONJSON (default)
      -j: Convert BONJSON to JSON
      -p: Pretty-print (if converting to JSON)
