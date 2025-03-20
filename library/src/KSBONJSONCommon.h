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

enum
{
    /* SMALLINT   96 = 0x60 */ TYPE_UINT8  = 0x70,   TYPE_STRING0  = 0x80,   TYPE_RESERVED_90 = 0x90,
    /* SMALLINT   97 = 0x61 */ TYPE_UINT16 = 0x71,   TYPE_STRING1  = 0x81,   TYPE_RESERVED_91 = 0x91,
    /* SMALLINT   98 = 0x62 */ TYPE_UINT24 = 0x72,   TYPE_STRING2  = 0x82,   TYPE_RESERVED_92 = 0x92,
    /* SMALLINT   99 = 0x63 */ TYPE_UINT32 = 0x73,   TYPE_STRING3  = 0x83,   TYPE_RESERVED_93 = 0x93,
    /* SMALLINT  100 = 0x64 */ TYPE_UINT40 = 0x74,   TYPE_STRING4  = 0x84,   TYPE_RESERVED_94 = 0x94,
    TYPE_RESERVED_65 = 0x65,   TYPE_UINT48 = 0x75,   TYPE_STRING5  = 0x85,   TYPE_RESERVED_95 = 0x95,
    TYPE_RESERVED_66 = 0x66,   TYPE_UINT56 = 0x76,   TYPE_STRING6  = 0x86,   TYPE_RESERVED_96 = 0x96,
    TYPE_RESERVED_67 = 0x67,   TYPE_UINT64 = 0x77,   TYPE_STRING7  = 0x87,   TYPE_RESERVED_97 = 0x97,
    TYPE_STRING      = 0x68,   TYPE_SINT8  = 0x78,   TYPE_STRING8  = 0x88,   TYPE_RESERVED_98 = 0x98,
    TYPE_BIG_NUMBER  = 0x69,   TYPE_SINT16 = 0x79,   TYPE_STRING9  = 0x89,   TYPE_ARRAY       = 0x99,
    TYPE_FLOAT16     = 0x6a,   TYPE_SINT24 = 0x7a,   TYPE_STRING10 = 0x8a,   TYPE_OBJECT      = 0x9a,
    TYPE_FLOAT32     = 0x6b,   TYPE_SINT32 = 0x7b,   TYPE_STRING11 = 0x8b,   TYPE_END         = 0x9b,
    TYPE_FLOAT64     = 0x6c,   TYPE_SINT40 = 0x7c,   TYPE_STRING12 = 0x8c,   /* SMALLINT -100 = 0x9c */
    TYPE_NULL        = 0x6d,   TYPE_SINT48 = 0x7d,   TYPE_STRING13 = 0x8d,   /* SMALLINT  -99 = 0x9d */
    TYPE_FALSE       = 0x6e,   TYPE_SINT56 = 0x7e,   TYPE_STRING14 = 0x8e,   /* SMALLINT  -98 = 0x9e */
    TYPE_TRUE        = 0x6f,   TYPE_SINT64 = 0x7f,   TYPE_STRING15 = 0x8f,   /* SMALLINT  -97 = 0x9f */
};

enum
{
    SMALLINT_NEGATIVE_EDGE = -100,
    SMALLINT_POSITIVE_EDGE = 100,
};


#ifdef __cplusplus
}
#endif

#endif // KSBONJSONCommon_h
