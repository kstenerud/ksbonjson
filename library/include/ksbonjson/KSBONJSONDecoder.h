//
//  KSBONJSONCodec.h
//
//  Created by Karl Stenerud on 2024-07-07.
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

#ifndef KSBONJSONDecoder_h
#define KSBONJSONDecoder_h

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>


// ============================================================================
// Compile-time Configuration
// ============================================================================

/**
 * Maximum depth of objects / arrays before the library will abort processing.
 * This increases encoder and decoder memory usage by 1 byte per level.
 */
#ifndef KSBONJSON_MAX_CONTAINER_DEPTH
#   define KSBONJSON_MAX_CONTAINER_DEPTH 200
#endif

/**
 * The restrict modifier, if available, increases optimization opportunities.
 */
#ifndef KSBONJSON_RESTRICT
#   ifdef __cplusplus
#       define KSBONJSON_RESTRICT __restrict__
#   else
#       define KSBONJSON_RESTRICT restrict
#   endif
#endif

/**
 * If your compiler makes symbols private by default, you will need to define this.
 */
#ifndef KSBONJSON_PUBLIC
#   if defined _WIN32 || defined __CYGWIN__
#       define KSBONJSON_PUBLIC __declspec(dllimport)
#   else
#       define KSBONJSON_PUBLIC
#   endif
#endif

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
#      error Unknown machine endianness detected. User needs to define KSBONJSON_IS_LITTLE_ENDIAN.
#    endif // __BYTE_ORDER__
// Detect with GLIBC's endian.h
#  elif defined(__GLIBC__)
#    include <endian.h>
#    if (__BYTE_ORDER == __LITTLE_ENDIAN)
#      define KSBONJSON_IS_LITTLE_ENDIAN 1
#    elif (__BYTE_ORDER == __BIG_ENDIAN)
#      define KSBONJSON_IS_LITTLE_ENDIAN 0
#    else
#      error Unknown machine endianness detected. User needs to define KSBONJSON_IS_LITTLE_ENDIAN.
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
#    error Unknown machine endianness detected. User needs to define KSBONJSON_IS_LITTLE_ENDIAN.
#  endif
#endif // KSBONJSON_IS_LITTLE_ENDIAN


// ============================================================================
// Header
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    /**
     * Everything completed without error
     */
    KSBONJSON_DECODE_OK = 0,
    
    /**
     * Source data appears to be truncated.
     */
    KSBONJSON_DECODE_INCOMPLETE = 1,

    /**
     * A field was too big or long.
     */
    KSBONJSON_DECODE_TOO_BIG = 2,

    /**
     * Not all containers have been closed yet (likely the document has been truncated).
     */
    KSBONJSON_DECODE_UNCLOSED_CONTAINERS = 3,

    /**
     * The document had too much container depth.
     */
    KSBONJSON_DECODE_CONTAINER_DEPTH_EXCEEDED = 4,

    /**
     * Tried to close too many containers.
     */
    KSBONJSON_DECODE_UNBALANCED_CONTAINERS = 5,

    /**
     * Expected to find a string for an object element name.
     */
    KSBONJSON_DECODE_EXPECTED_OBJECT_NAME = 6,

    /**
     * Got an end container while expecting an object element value.
     */
    KSBONJSON_DECODE_EXPECTED_OBJECT_VALUE = 7,

    /**
     * Generic error code that can be returned from a callback.
     *
     * More specific error codes (> 100) may also be defined by the user if needed.
     */
    KSBONJSON_DECODE_COULD_NOT_PROCESS_DATA = 100,
} ksbonjson_decodeStatus;

/**
 * Callbacks called during a BONJSON decode process.
 * All function pointers must point to valid functions.
 */
typedef struct KSBONJSONDecodeCallbacks
{
    /**
     * Called when a boolean element value is decoded.
     *
     * @param value The element's value.
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onBoolean)(bool value, void* userData);

    /**
     * Called when an integer element value is decoded.
     *
     * @param value The element's value.
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onInteger)(int64_t value, void* userData);

    /**
     * Called when an integer element value is decoded.
     *
     * @param value The element's value.
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onUInteger)(uint64_t value, void* userData);

    /**
     * Called when a floating point element value is decoded.
     *
     * @param value The element's value.
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onFloat)(double value, void* userData);

    /**
     * Called when a null element value is decoded.
     *
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onNull)(void* userData);

    /**
     * Called when a string element value is decoded.
     *
     * @param value The element's value.
     * @param valueLength The value's length.
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onString)(const char* KSBONJSON_RESTRICT value,
                                       size_t valueLength,
                                       void* KSBONJSON_RESTRICT userData);

    /**
     * Called when a new object is encountered.
     *
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onBeginObject)(void* userData);

    /**
     * Called when a new array is encountered.
     *
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onBeginArray)(void* userData);

    /**
     * Called when leaving the current container and returning to the next
     * higher level container.
     *
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onEndContainer)(void* userData);

    /**
     * Called when the end of the document is reached.
     *
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onEndData)(void* userData);

} KSBONJSONDecodeCallbacks;


// ============================================================================
// API
// ============================================================================

/**
 * Decode a BONJSON document.
 *
 * @param document The document to decode.
 * @param documentLength The length of the document.
 * @param callbacks The callbacks to call with events as the document is decoded.
 * @param userData Any user-defined data you want passed to the callbacks.
 * @param decodedOffset Pointer to a variable that will hold the offset to where decoding stopped.
 * @return KSBONJSON_DECODE_OK on success.
 */
KSBONJSON_PUBLIC ksbonjson_decodeStatus ksbonjson_decode(const uint8_t* KSBONJSON_RESTRICT document,
                                                         size_t documentLength,
                                                         const KSBONJSONDecodeCallbacks* KSBONJSON_RESTRICT callbacks,
                                                         void* KSBONJSON_RESTRICT userData,
                                                         size_t* KSBONJSON_RESTRICT decodedOffset);

/**
 * Get a description for a decoding status code.
 *
 * @param status The status code.
 *
 * @return A statically allocated string describing the status.
 */
KSBONJSON_PUBLIC const char* ksbonjson_decodeStatusDescription(ksbonjson_decodeStatus status);


#ifdef __cplusplus
}
#endif

#endif // KSBONJSONDecoder_h
