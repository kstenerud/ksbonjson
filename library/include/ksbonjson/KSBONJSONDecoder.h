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

#ifndef KSBONJSON_MAX_CONTAINER_DEPTH
    #define KSBONJSON_MAX_CONTAINER_DEPTH 200
#endif

#ifndef KSBONJSON_DECODE_MAX_CHUNKED_NAME_LENGTH
    #define KSBONJSON_DECODE_MAX_CHUNKED_NAME_LENGTH 1000
#endif

#ifndef KSBONJSON_RESTRICT
    #ifdef __cplusplus
        #define KSBONJSON_RESTRICT __restrict__
    #else
        #define KSBONJSON_RESTRICT restrict
    #endif
#endif

#ifndef KSBONJSON_PUBLIC
    #if defined _WIN32 || defined __CYGWIN__
        #define KSBONJSON_PUBLIC __declspec(dllimport)
    #else
        #define KSBONJSON_PUBLIC
    #endif
#endif


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
    KSBONJSON_DECODE_EXPECTED_STRING = 6,

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
     * Called when an object element's name is decoded.
     *
     * @param name The element's name.
     * @param nameLength The length of the name.
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onName)(const char* KSBONJSON_RESTRICT name,
                                     size_t nameLength,
                                     void* KSBONJSON_RESTRICT userData);

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
     * Called when a chunk of a string element value is decoded.
     *
     * @param chunk The chunk's value.
     * @param chunkLength The chunk's length.
     * @param isLastChunk If true, this is the last chunk for this string element.
     * @param userData Data that was specified when calling ksbonjson_decode().
     * @return KSBONJSON_DECODE_OK if decoding should continue.
     */
    ksbonjson_decodeStatus (*onStringChunk)(const char* KSBONJSON_RESTRICT chunk,
                                            size_t chunkLength,
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
