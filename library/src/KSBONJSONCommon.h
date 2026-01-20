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
// ABOUTME: Type codes are arranged for efficient masking: integers at 0xd0-0xdf,
// ABOUTME: strings at 0xe0-0xf0, other types at 0xf1-0xf9.

enum
{
    // Small integers: 0x00-0xc8 encode values -100 to 100 (value = type_code - 100)
    // SMALLINT -100 = 0x00, SMALLINT 0 = 0x64, SMALLINT 100 = 0xc8

    // Reserved: 0xc9-0xcf
    TYPE_RESERVED_C9 = 0xc9,
    TYPE_RESERVED_CA = 0xca,
    TYPE_RESERVED_CB = 0xcb,
    TYPE_RESERVED_CC = 0xcc,
    TYPE_RESERVED_CD = 0xcd,
    TYPE_RESERVED_CE = 0xce,
    TYPE_RESERVED_CF = 0xcf,

    // Unsigned integers: 0xd0-0xd7 (1-8 bytes)
    TYPE_UINT8  = 0xd0,
    TYPE_UINT16 = 0xd1,
    TYPE_UINT24 = 0xd2,
    TYPE_UINT32 = 0xd3,
    TYPE_UINT40 = 0xd4,
    TYPE_UINT48 = 0xd5,
    TYPE_UINT56 = 0xd6,
    TYPE_UINT64 = 0xd7,

    // Signed integers: 0xd8-0xdf (1-8 bytes)
    TYPE_SINT8  = 0xd8,
    TYPE_SINT16 = 0xd9,
    TYPE_SINT24 = 0xda,
    TYPE_SINT32 = 0xdb,
    TYPE_SINT40 = 0xdc,
    TYPE_SINT48 = 0xdd,
    TYPE_SINT56 = 0xde,
    TYPE_SINT64 = 0xdf,

    // Short strings: 0xe0-0xef (0-15 bytes)
    TYPE_STRING0  = 0xe0,
    TYPE_STRING1  = 0xe1,
    TYPE_STRING2  = 0xe2,
    TYPE_STRING3  = 0xe3,
    TYPE_STRING4  = 0xe4,
    TYPE_STRING5  = 0xe5,
    TYPE_STRING6  = 0xe6,
    TYPE_STRING7  = 0xe7,
    TYPE_STRING8  = 0xe8,
    TYPE_STRING9  = 0xe9,
    TYPE_STRING10 = 0xea,
    TYPE_STRING11 = 0xeb,
    TYPE_STRING12 = 0xec,
    TYPE_STRING13 = 0xed,
    TYPE_STRING14 = 0xee,
    TYPE_STRING15 = 0xef,

    // Long string: 0xf0
    TYPE_STRING = 0xf0,

    // Big number: 0xf1
    TYPE_BIG_NUMBER = 0xf1,

    // Floats: 0xf2-0xf4
    TYPE_FLOAT16 = 0xf2,
    TYPE_FLOAT32 = 0xf3,
    TYPE_FLOAT64 = 0xf4,

    // Null, Boolean: 0xf5-0xf7
    TYPE_NULL  = 0xf5,
    TYPE_FALSE = 0xf6,
    TYPE_TRUE  = 0xf7,

    // Containers: 0xf8-0xf9
    TYPE_ARRAY  = 0xf8,
    TYPE_OBJECT = 0xf9,

    // Reserved: 0xfa-0xff
    TYPE_RESERVED_FA = 0xfa,
    TYPE_RESERVED_FB = 0xfb,
    TYPE_RESERVED_FC = 0xfc,
    TYPE_RESERVED_FD = 0xfd,
    TYPE_RESERVED_FE = 0xfe,
    TYPE_RESERVED_FF = 0xff,
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
