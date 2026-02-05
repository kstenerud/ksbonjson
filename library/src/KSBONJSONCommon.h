//
//  KSBONJSONCommon.h
//
//  Created by Karl Stenerud on 2025-03-15.
//
//  Copyright (c) 2024 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#ifndef KSBONJSONCommon_h
#define KSBONJSONCommon_h

#ifdef __cplusplus
extern "C" {
#endif


// ============================================================================
// Helpers
// ============================================================================

/**
 * Best-effort attempt to get the endianness of the machine being compiled for.
 * If this fails, you will have to define it manually.
 *
 * Shamelessly stolen from https://github.com/Tencent/rapidjson/blob/master/include/rapidjson/rapidjson.h
 */
#ifndef KSBONJSON_IS_LITTLE_ENDIAN
// Detect with GCC 4.6's macro
#  ifdef __BYTE_ORDER__
#    if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#      define KSBONJSON_IS_LITTLE_ENDIAN 1
#    elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#      define KSBONJSON_IS_LITTLE_ENDIAN 0
#    else
#      error Could not auto-detect machine endianness. Please define KSBONJSON_IS_LITTLE_ENDIAN (0 or 1), or _LITTLE_ENDIAN or _BIG_ENDIAN.
#    endif // __BYTE_ORDER__
// Detect with GLIBC's endian.h
#  elif defined(__GLIBC__)
#    include <endian.h>
#    if (__BYTE_ORDER == __LITTLE_ENDIAN)
#      define KSBONJSON_IS_LITTLE_ENDIAN 1
#    elif (__BYTE_ORDER == __BIG_ENDIAN)
#      define KSBONJSON_IS_LITTLE_ENDIAN 0
#    else
#      error Could not auto-detect machine endianness. Please define KSBONJSON_IS_LITTLE_ENDIAN (0 or 1), or _LITTLE_ENDIAN or _BIG_ENDIAN.
#   endif // __GLIBC__
// Detect with _LITTLE_ENDIAN and _BIG_ENDIAN macro
#  elif defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
#    define KSBONJSON_IS_LITTLE_ENDIAN 1
#  elif defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)
#    define KSBONJSON_IS_LITTLE_ENDIAN 0
// Detect with architecture macros
#  elif defined(__sparc) || defined(__sparc__) || defined(_POWER) || defined(__powerpc__) || defined(__ppc__) || defined(__ppc64__) || defined(__hpux) || defined(__hppa) || defined(_MIPSEB) || defined(_POWER) || defined(__s390__)
#    define KSBONJSON_IS_LITTLE_ENDIAN 0
#  elif defined(__i386__) || defined(__alpha__) || defined(__ia64) || defined(__ia64__) || defined(_M_IX86) || defined(_M_IA64) || defined(_M_ALPHA) || defined(__amd64) || defined(__amd64__) || defined(_M_AMD64) || defined(__x86_64) || defined(__x86_64__) || defined(_M_X64) || defined(__bfin__)
#    define KSBONJSON_IS_LITTLE_ENDIAN 1
#  elif defined(_MSC_VER) && (defined(_M_ARM) || defined(_M_ARM64))
#    define KSBONJSON_IS_LITTLE_ENDIAN 1
#  else
#    error Could not auto-detect machine endianness. Please define KSBONJSON_IS_LITTLE_ENDIAN (0 or 1), or _LITTLE_ENDIAN or _BIG_ENDIAN.
#  endif
#endif // KSBONJSON_IS_LITTLE_ENDIAN

#ifndef HAS_BUILTIN
#   ifdef _MSC_VER
#       define HAS_BUILTIN(A) 0
#   else
#       define HAS_BUILTIN(A) __has_builtin(A)
#   endif
#endif

// Compiler hints for "if" statements
#ifndef likely_if
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-macros"
#   if HAS_BUILTIN(__builtin_expect)
#       define likely_if(x) if(__builtin_expect(x,1))
#       define unlikely_if(x) if(__builtin_expect(x,0))
#   else
#       define likely_if(x) if(x)
#       define unlikely_if(x) if(x)
#   endif
#   pragma GCC diagnostic pop
#endif


// ============================================================================
// Constants
// ============================================================================

// ABOUTME: Type codes for BONJSON encoding.
// ABOUTME: Small integers 0x00-0xc8 encode values -100 to 100 (type_code - 100).
// ABOUTME: Type codes: 0xca-0xcf scalars, 0xd0-0xdf short strings,
// ABOUTME: 0xe0-0xe7 integers (CPU-native sizes), 0xfc-0xff containers/delimiters.

enum
{
    // Small integers: 0x00-0xc8 encode values -100 to 100 (value = type_code - 100)
    // SMALLINT -100 = 0x00, SMALLINT 0 = 0x64, SMALLINT 100 = 0xc8

    // Reserved: 0xc9
    TYPE_RESERVED_C9 = 0xc9,

    // Big number (zigzag LEB128 metadata + LE magnitude): 0xca
    TYPE_BIG_NUMBER = 0xca,

    // Floats: 0xcb-0xcc (CPU-native sizes only, no bfloat16)
    TYPE_FLOAT32 = 0xcb,
    TYPE_FLOAT64 = 0xcc,

    // Null, Boolean: 0xcd-0xcf
    TYPE_NULL  = 0xcd,
    TYPE_FALSE = 0xce,
    TYPE_TRUE  = 0xcf,

    // Short strings: 0xd0-0xdf (0-15 bytes)
    TYPE_STRING0  = 0xd0,
    TYPE_STRING1  = 0xd1,
    TYPE_STRING2  = 0xd2,
    TYPE_STRING3  = 0xd3,
    TYPE_STRING4  = 0xd4,
    TYPE_STRING5  = 0xd5,
    TYPE_STRING6  = 0xd6,
    TYPE_STRING7  = 0xd7,
    TYPE_STRING8  = 0xd8,
    TYPE_STRING9  = 0xd9,
    TYPE_STRING10 = 0xda,
    TYPE_STRING11 = 0xdb,
    TYPE_STRING12 = 0xdc,
    TYPE_STRING13 = 0xdd,
    TYPE_STRING14 = 0xde,
    TYPE_STRING15 = 0xdf,

    // Unsigned integers: 0xe0-0xe3 (CPU-native sizes: 1, 2, 4, 8 bytes)
    TYPE_UINT8  = 0xe0,
    TYPE_UINT16 = 0xe1,
    TYPE_UINT32 = 0xe2,
    TYPE_UINT64 = 0xe3,

    // Signed integers: 0xe4-0xe7 (CPU-native sizes: 1, 2, 4, 8 bytes)
    TYPE_SINT8  = 0xe4,
    TYPE_SINT16 = 0xe5,
    TYPE_SINT32 = 0xe6,
    TYPE_SINT64 = 0xe7,

    // Reserved: 0xe8-0xfb

    // Containers: 0xfc-0xfd (terminated by TYPE_END)
    TYPE_ARRAY  = 0xfc,
    TYPE_OBJECT = 0xfd,

    // Container end marker
    TYPE_END = 0xfe,

    // Long string delimiter (0xff + data + 0xff)
    TYPE_STRINGL = 0xff,
};

enum
{
    SMALLINT_MIN = -100,
    SMALLINT_MAX = 100,
    SMALLINT_BIAS = 100,  // type_code = value + SMALLINT_BIAS
    SMALLINT_MAX_TYPE_CODE = 0xc8,  // type_code for value 100
};


#ifdef __cplusplus
}
#endif

#endif // KSBONJSONCommon_h
