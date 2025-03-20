Reference Library for BONJSON
=============================

A C implementation to demonstrate a simple [BONJSON](https://github.com/kstenerud/bonjson/blob/main/bonjson.md) codec.

This library has no external dependencies, makes no allocations, and calls no external functions beyond `memcpy()` and some GCC intrinsics (if available).

This is not intended as a complete solution; rather, this library handles the encoding and decoding, and the calling environment (such as a wrapper library) handles certain validation tasks (such as UTF-8, or duplicate name checking), which avoids tying this library to a particular implementation.


Security Rules
--------------

The BONJSON spec mandates a number of [security rules](https://github.com/kstenerud/bonjson/blob/main/bonjson.md#security-rules).

**The following security rules are implemented in this library**:

* NaN and infinity are rejected
* Values that are larger than can fit in 64 bits are rejected (for BigNumber, up to 64 bits of significand data)

**The following security rules are **NOT** implemented (you must implement them in your wrapper library)**:

* Maximum lengths
* UTF-8 validation
* Objects with duplicate names
* Chunking restrictions


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
