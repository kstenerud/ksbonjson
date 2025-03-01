Reference Library for BONJSON
=============================

A C implementation to demonstrate a simple [BONJSON](https://github.com/kstenerud/bonjson/blob/main/bonjson.md) codec.

This library has no external dependencies, makes no allocations, and calls no external functions beyond `memcpy()`.

Limitations
-----------

Only supports Big Number with a significand up to 64 bits long.


Requirements
------------

  * Meson 0.49 or newer
  * Ninja 1.8.2 or newer
  * A C compiler
  * A C++ compiler (for the tests)


Building
--------

    meson build
    ninja -C build


Running Tests
-------------

    ninja -C build test

For the full report:

    ./build/run_tests


Installing
----------

    ninja -C build install


Usage
-----

### Decoding

* Set up a `KSBONJSONDecodeCallbacks` structure
* Load the document into memory
* Call `ksbonjson_decode()`

### Encoding

* Build a "write" callback function that conforms to `KSBONJSONAddEncodedDataFunc`
* Instantiate a `KSBONJSONEncodeContext`
* Call `ksbonjson_beginEncode()`
* Call the various `ksbonjson_addXYZ()` and `ksbonjson_beginXYZ()` API methods to construct your document
* Call `ksbonjson_endEncode()` when you're done

### Example

See [the example JSON<>BONJSON converter](../executable) for a full, working example of how to use this library.
