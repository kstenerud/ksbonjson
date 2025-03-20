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

#ifndef KSBONJSONEncoder_h
#define KSBONJSONEncoder_h

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif


// ============================================================================
// Compile-time Configuration (synced with decoder)
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
// Common Types (synced with decoder)
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
// Encoder Types
// ============================================================================

typedef enum
{
    /**
     * Everything completed without error
     */
    KSBONJSON_ENCODE_OK = 0,
    
    /**
     * Expected an object element name, but got a non-string.
     */
    KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME = 1,
    
    /**
     * Attempted to close an object while it's expecting a value for the current name.
     */
    KSBONJSON_ENCODE_EXPECTED_OBJECT_VALUE = 2,

    /**
     * Attempted to add a discrete value while chunking a string.
     */
    KSBONJSON_ENCODE_CHUNKING_STRING = 3,
    
    /**
     * Passed in a NULL pointer.
     */
    KSBONJSON_ENCODE_NULL_POINTER = 4,
    
    /**
     * Attempted to close more containers than there actually are.
     */
    KSBONJSON_ENCODE_CLOSED_TOO_MANY_CONTAINERS = 5,

    /**
     * Attempted to end the encoding while there are still containers open.
     */
    KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN = 6,

    /**
     * The object to encode contains invalid data.
     */
    KSBONJSON_ENCODE_INVALID_DATA = 7,

    /**
     * Passed in data was too big or long.
     */
    KSBONJSON_ENCODE_TOO_BIG = 8,

    /**
     * Generic error code that can be returned from addEncodedData().
     *
     * More specific error codes (> 100) may also be defined by the user if needed.
     */
    KSBONJSON_ENCODE_COULD_NOT_ADD_DATA = 100,
} ksbonjson_encodeStatus;

/**
 * Function pointer for adding more encoded binary data to the document.
 *
 * @param data The binary data to add.
 * @param dataLength The length of the data.
 * @param userData user-specified contextual data.
 * @return KSBONJSON_ENCODER_OK if the operation was successful.
 */
typedef ksbonjson_encodeStatus (*KSBONJSONAddEncodedDataFunc)(const uint8_t* KSBONJSON_RESTRICT data,
                                                              size_t dataLength,
                                                              void* KSBONJSON_RESTRICT userData);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
typedef struct
{
    uint8_t isObject: 1;
    uint8_t isExpectingName: 1;
    uint8_t isChunkingString: 1;
} KSBONJSONContainerState;

typedef struct
{
    KSBONJSONAddEncodedDataFunc addEncodedData;
    void* userData;
    int containerDepth;
    KSBONJSONContainerState containers[KSBONJSON_MAX_CONTAINER_DEPTH];
} KSBONJSONEncodeContext;
#pragma GCC diagnostic pop


// ============================================================================
// Encoder API
// ============================================================================

/**
 * Begin a new encoding process.
 *
 * @param context The encoding context.
 * @param addEncodedData Function to handle adding data after it's been encoded.
 * @param userData User-specified data which gets passed to addEncodedData.
 */
KSBONJSON_PUBLIC void ksbonjson_beginEncode(KSBONJSONEncodeContext* context,
                                            KSBONJSONAddEncodedDataFunc addEncodedData,
                                            void* userData);

/**
 * End the encoding process.
 *
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_endEncode(KSBONJSONEncodeContext* context);

/**
 * End all open containers in the document, leaving it in a balanced state ready for ksbonjson_endEncode().
 *
 * @param context The encoding context.
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_terminateDocument(KSBONJSONEncodeContext* context);

/**
 * Add a boolean element.
 *
 * @param context The encoding context.
 * @param value The element's value.
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_addBoolean(KSBONJSONEncodeContext* context, bool value);

/**
 * Add an unsigned integer element.
 *
 * @param context The encoding context.
 * @param value The element's value.
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_addUnsignedInteger(KSBONJSONEncodeContext* context, uint64_t value);

/**
 * Add a signed integer element.
 *
 * @param context The encoding context.
 * @param value The element's value.
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_addSignedInteger(KSBONJSONEncodeContext* context, int64_t value);

/**
 * Add a floating point element.
 *
 * @param context The encoding context.
 * @param value The element's value.
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_addFloat(KSBONJSONEncodeContext* context, double value);

/**
 * Add a Big Number element.
 *
 * @param context The encoding context.
 * @param value The element's value.
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_addBigNumber(KSBONJSONEncodeContext* context, KSBigNumber value);

/**
 * Add a null element.
 *
 * @param context The encoding context.
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_addNull(KSBONJSONEncodeContext* context);

/**
 * Add a string element.
 *
 * Note: This library doesn't do UTF-8 checking!
 *
 * @param context The encoding context.
 * @param value The element's value. This MUST be a complete and valid UTF-8 string!
 * @param valueLength the length of the string.
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_addString(KSBONJSONEncodeContext* KSBONJSON_RESTRICT context,
                                                            const char* KSBONJSON_RESTRICT value,
                                                            size_t valueLength);

/**
 * Build a string element progressively in chunks. When isLastChunk is true, the string is considered complete.
 *
 * Note: This library doesn't do UTF-8 checking!
 *
 * @param context The encoding context.
 * @param chunk The string chunk. This MUST be a complete and valid UTF-8 string!
 * @param chunkLength the length of the string chunk.
 * @param isLastChunk set to true if this chunk also marks the end of the string element.
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_chunkString(KSBONJSONEncodeContext* KSBONJSON_RESTRICT context,
                                                              const char* KSBONJSON_RESTRICT chunk,
                                                              size_t chunkLength,
                                                              bool isLastChunk);

/** Add an already encoded BONJSON document as an element.
 *
 * @param context The encoding context.
 * @param bonjsonDocument A valid, encoded BONJSON document.
 * @param documentLength The length of the BONJSON document.
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_addBONJSONDocument(KSBONJSONEncodeContext* KSBONJSON_RESTRICT context,
                                                                     const uint8_t* KSBONJSON_RESTRICT bonjsonDocument,
                                                                     size_t documentLength);

/**
 * Begin a new object container.
 *
 * @param context The encoding context.
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_beginObject(KSBONJSONEncodeContext* context);

/**
 * Begin a new array container.
 *
 * @param context The encoding context.
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_beginArray(KSBONJSONEncodeContext* context);

/**
 * End the current container and return to the next higher level.
 *
 * @param context The encoding context.
 * @return KSBONJSON_ENCODER_OK if the process was successful.
 */
KSBONJSON_PUBLIC ksbonjson_encodeStatus ksbonjson_endContainer(KSBONJSONEncodeContext* context);

/**
 * Get a description for an encoding status code.
 *
 * @param status The status code.
 *
 * @return A statically allocated string describing the status.
 */
KSBONJSON_PUBLIC const char* ksbonjson_describeEncodeStatus(ksbonjson_encodeStatus status) __attribute__((const));


// ============================================================================
// End
// ============================================================================

#ifdef __cplusplus
}
#endif


#endif /* KSBONJSONEncoder_h */
