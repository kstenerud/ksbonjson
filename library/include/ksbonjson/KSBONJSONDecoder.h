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


#ifdef __cplusplus
extern "C" {
#endif


// ============================================================================
// Compile-time Configuration (synced with encoder)
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


// ============================================================================
// Common Types (synced with encoder)
// ============================================================================

#ifndef TYPEDEF_KSBIGNUMBER
#define TYPEDEF_KSBIGNUMBER
    typedef struct
    {
        uint64_t significand;     // Unsigned 64-bit absolute value
        int32_t exponent;         // Signed 24-bit (-0x800000 to 0x7fffff)
        int32_t significandSign; // 1 or -1
    } KSBigNumber;

    /**
     * Create a new Big Number
     * @param sign The significand's sign: 1 (positive) or -1 (negative)
     * @param significandAbs The absolute value of the significand
     * @param exponent The exponent (-0x800000 to 0x7fffff)
     * @return A new Big Number
     */
    static inline KSBigNumber ksbonjson_newBigNumber(int sign, uint64_t significandAbs, int32_t exponent)
    {
        KSBigNumber v =
                {
                    .significand = significandAbs,
                    .exponent = exponent,
                    .significandSign = (int32_t)sign,
                };
        return v;
    }
#endif // TYPEDEF_KSBIGNUMBER


// ============================================================================
// Decoder Types
// ============================================================================

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
     * Not all containers have been closed yet (likely the document has been truncated).
     */
    KSBONJSON_DECODE_UNCLOSED_CONTAINERS = 2,

    /**
     * Tried to close too many containers.
     */
    KSBONJSON_DECODE_UNBALANCED_CONTAINERS = 3,

    /**
     * The document had too much container depth.
     */
    KSBONJSON_DECODE_CONTAINER_DEPTH_EXCEEDED = 4,

    /**
     * Expected to find a string for an object element name.
     */
    KSBONJSON_DECODE_EXPECTED_OBJECT_NAME = 5,

    /**
     * Got an end container while expecting an object element value.
     */
    KSBONJSON_DECODE_EXPECTED_OBJECT_VALUE = 6,

    /**
     * An element was successfully decoded, but contained invalid data.
     */
    KSBONJSON_DECODE_INVALID_DATA = 7,

    /**
     * This name already exists in the current object.
     */
    KSBONJSON_DECODE_DUPLICATE_OBJECT_NAME = 8,

    /**
     * The value is out of range and cannot be stored without data loss.
     */
    KSBONJSON_DECODE_VALUE_OUT_OF_RANGE = 9,

    /**
     * A string value contained a NUL character.
     */
    KSBONJSON_DECODE_NUL_CHARACTER = 10,

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
     * Called when an unsigned integer element value is decoded.
     *
     * @param value The element's value.
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onUnsignedInteger)(uint64_t value, void* userData);

    /**
     * Called when a signed integer element value is decoded.
     *
     * @param value The element's value.
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onSignedInteger)(int64_t value, void* userData);

    /**
     * Called when a floating point element value is decoded.
     *
     * @param value The element's value.
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onFloat)(double value, void* userData);

    /**
     * Called when a Big Number element value is decoded.
     *
     * @param value The element's value.
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onBigNumber)(KSBigNumber value, void* userData);

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
     * The string data has NOT been validated against UTF-8!
     *
     * Even after validation, the string data could in theory contain NUL
     * characters (which are valid in JSON).
     *
     * @param value The element's value.
     * @param length The value's length.
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onString)(const char* KSBONJSON_RESTRICT value,
                                       size_t length,
                                       void* KSBONJSON_RESTRICT userData);

    /**
     * Called when a string chunk is decoded.
     *
     * The BONSJON spec requires at least one chunking security policy.
     * As this library is a low-level building block, it's on the user of this library
     * to add such a policy.
     *
     * The string data has NOT been validated against UTF-8!
     *
     * Even after validation, the string data could in theory contain NUL
     * characters (which are valid in JSON).
     *
     * @param value The chunk's value.
     * @param length The chunk's length in bytes.
     * @param isLastChunk If true, this is the last chunk, and the string is done.
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onStringChunk)(const char* KSBONJSON_RESTRICT value,
        size_t length,
        bool isLastChunk,
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
// Decoder API
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
KSBONJSON_PUBLIC const char* ksbonjson_describeDecodeStatus(ksbonjson_decodeStatus status) __attribute__((const));


// ============================================================================
// End
// ============================================================================

#ifdef __cplusplus
}
#endif

#endif // KSBONJSONDecoder_h
