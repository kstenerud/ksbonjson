# ksbonjson

C library for encoding and decoding BONJSON (Binary Object Notation for JSON).

## Architecture

The library is a low-level C codec with a SAX-style callback API. It consists of:

- **Encoder** (`KSBONJSONEncoder.c/h`): Converts values to BONJSON binary format via a push API. Caller provides an `addEncodedData` callback to receive encoded bytes.
- **Decoder** (`KSBONJSONDecoder.c/h`): Reads BONJSON binary and fires callbacks for each decoded value (SAX-style). Single-pass, no memory allocation.
- **Common** (`KSBONJSONCommon.h`): Internal header with type code definitions and endianness helpers.

## BONJSON Format

### Type Codes

| Range | Meaning |
|-------|---------|
| 0x00-0xC8 | Small integers (-100 to 100, value = type_code - 100) |
| 0xC9 | Reserved |
| 0xCA | BigNumber (zigzag LEB128 exponent + zigzag LEB128 signed_length + LE magnitude bytes) |
| 0xCB | Float32 (4 bytes little-endian) |
| 0xCC | Float64 (8 bytes little-endian) |
| 0xCD | Null |
| 0xCE | False |
| 0xCF | True |
| 0xD0-0xDF | Short strings (0-15 bytes inline) |
| 0xE0-0xE3 | Unsigned integers (1, 2, 4, 8 bytes - CPU-native sizes only) |
| 0xE4-0xE7 | Signed integers (1, 2, 4, 8 bytes - CPU-native sizes only) |
| 0xE8-0xFB | Reserved |
| 0xFC | Array (children terminated by 0xFE) |
| 0xFD | Object (key-value pairs terminated by 0xFE) |
| 0xFE | Container end marker |
| 0xFF | Long string delimiter (0xFF + data + 0xFF) |

### Key Design Decisions

- **Delimiter-terminated containers**: Arrays and objects are terminated by 0xFE rather than length-prefixed. This simplifies streaming.
- **CPU-native integer sizes**: Only 1, 2, 4, 8 byte integers (no 3, 5, 6, 7). Values are rounded up to the next native size.
- **Signed preferred**: When encoding positive values, signed type is preferred over unsigned when both would use the same number of bytes.
- **Long strings use 0xFF delimiter**: UTF-8 never contains 0xFF, so it's safe as a sentinel. Format: `0xFF <data> 0xFF`.
- **BigNumber uses zigzag LEB128 + LE magnitude**: Exponent is zigzag LEB128, followed by a signed_length (zigzag LEB128) whose absolute value is the magnitude byte count and whose sign is the significand sign, followed by the magnitude as unsigned LE bytes. Zero significand has signed_length=0 with no magnitude bytes. Magnitude must be normalized (last byte non-zero).
- **Float optimization**: Floats that are exact integers are encoded as integers. Float64 values that fit in Float32 without loss use Float32.

## Building

Uses meson build system. Build directory is `library/build/`.

```
cd library/build && ninja
```

## Testing

Tests use Google Test and are in `library/tests/src/tests.cpp`. Run with:

```
cd library/build && ./run_tests
```

Test helpers:
- `events.h`: Event classes that can both encode (push API) and compare decoded events.
- `encoder.h`: C++ wrapper around the C encoder.
- `decoder.h`: C++ wrapper around the C decoder with virtual callback methods.
- `assert_encode_decode()`: The primary test function - encodes events, verifies bytes, decodes, verifies events match, re-encodes to verify round-trip.

## Components

### Encoder (`KSBONJSONEncoder.c`)
- Tracks container state (object vs array, expecting key vs value) in a stack.
- `onValueAdded()` handles object key/value alternation after each value.
- Integer encoding picks the smallest CPU-native size, preferring signed when sizes are equal.
- `encodeZigzagLEB128()` handles BigNumber exponent and signed_length encoding. Magnitude is emitted as raw LE bytes.

### Decoder (`KSBONJSONDecoder.c`)
- Single-pass decoder reads type codes and dispatches to handlers.
- Container end (0xFE) is checked before type dispatch in the main loop.
- Long strings use `memchr()` to find the 0xFF terminator efficiently.
- `decodeZigzagLEB128()` handles BigNumber exponent and signed_length decoding. Magnitude is read as raw LE bytes with normalization validation.
