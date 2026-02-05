//
//  KSBONJSONCodec.c
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

// ABOUTME: BONJSON decoder implementation.
// ABOUTME: Decodes BONJSON binary format with delimiter-terminated
// ABOUTME: containers and CPU-native integer sizes.

#include "KSBONJSONDecoder.h"
#include "KSBONJSONCommon.h"
#include <string.h> // For memcpy() and memchr()
#ifdef _MSC_VER
#include <intrin.h>
#endif

#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"


// ============================================================================
// Types
// ============================================================================

union number_bits
{
    uint8_t  b[8];
    uint32_t u32;
    uint64_t u64;
    int64_t  i64;
    float    f32;
    double   f64;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
typedef struct
{
    uint8_t isObject: 1;
    uint8_t isExpectingName: 1;
} ContainerState;

typedef struct
{
    const uint8_t* bufferCurrent;
    const uint8_t* const bufferEnd;
    const KSBONJSONDecodeCallbacks* const callbacks;
    void* const userData;
    int containerDepth;
    ContainerState containers[KSBONJSON_MAX_CONTAINER_DEPTH];
} DecodeContext;
#pragma GCC diagnostic pop


// ============================================================================
// Macros
// ============================================================================

#define PROPAGATE_ERROR(CONTEXT, CALL) \
    do \
    { \
        const ksbonjson_decodeStatus propagatedResult = CALL; \
        unlikely_if(propagatedResult != KSBONJSON_DECODE_OK) \
        { \
            return propagatedResult; \
        } \
    } \
    while(0)

#define SHOULD_HAVE_ROOM_FOR_BYTES(BYTE_COUNT) \
    unlikely_if(ctx->bufferCurrent + (BYTE_COUNT) > ctx->bufferEnd) \
        return KSBONJSON_DECODE_INCOMPLETE


// ============================================================================
// Utility
// ============================================================================

static uint64_t fromLittleEndian(uint64_t v)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
    return v;
#else
    // Most compilers optimize this to a byte-swap instruction
    return (v>>56) | ((v&0x00ff000000000000ULL)>>40) | ((v&0x0000ff0000000000ULL)>>24) |
           ((v&0x000000ff00000000ULL)>> 8) | ((v&0x00000000ff000000ULL)<< 8) |
           (v<<56) | ((v&0x000000000000ff00ULL)<<40) | ((v&0x0000000000ff0000ULL)<<24);
#endif
}

/**
 * Decode a primitive numeric type of the specified size.
 * @param ctx The context
 * @param byteCount Size of the number in bytes. Do NOT set size > 8 as it isn't sanity checked!
 * @param initValue 0 for floats or positive ints, -1 for negative ints.
 */
static union number_bits decodePrimitiveNumeric(DecodeContext* const ctx,
                                                const size_t byteCount,
                                                const int64_t initValue)
{
    union number_bits bits = {.u64 = (uint64_t)initValue};
    const uint8_t* buf = ctx->bufferCurrent;
    ctx->bufferCurrent += byteCount;
    memcpy(bits.b, buf, byteCount);
    bits.u64 = fromLittleEndian(bits.u64);
    return bits;
}

static int8_t fillWithBit7(uint8_t value)
{
    return (int8_t)value >> 7;
}

static uint64_t decodeUnsignedInt(DecodeContext* const ctx, const size_t size)
{
    return decodePrimitiveNumeric(ctx, size, 0).u64;
}

static int64_t decodeSignedInt(DecodeContext* const ctx, const size_t size)
{
    return decodePrimitiveNumeric(ctx, size, fillWithBit7(ctx->bufferCurrent[size-1])).i64;
}

static float decodeFloat32(DecodeContext* const ctx)
{
    return decodePrimitiveNumeric(ctx, 4, 0).f32;
}

static double decodeFloat64(DecodeContext* const ctx)
{
    return decodePrimitiveNumeric(ctx, 8, 0).f64;
}

static ksbonjson_decodeStatus reportFloat(DecodeContext* const ctx, const double value)
{
    const union number_bits bits = {.f64 = value};
    unlikely_if((bits.u64 & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL)
    {
        // When all exponent bits are set, it signifies an infinite or NaN value
        return KSBONJSON_DECODE_INVALID_DATA;
    }
    return ctx->callbacks->onFloat(value, ctx->userData);
}

/**
 * Map unsigned integer type code index (0-3) to byte count (1, 2, 4, 8).
 */
static size_t unsignedIntTypeCodeToSize(uint8_t typeCode)
{
    static const size_t sizes[] = {1, 2, 4, 8};
    return sizes[typeCode - TYPE_UINT8];
}

/**
 * Map signed integer type code index (0-3) to byte count (1, 2, 4, 8).
 */
static size_t signedIntTypeCodeToSize(uint8_t typeCode)
{
    static const size_t sizes[] = {1, 2, 4, 8};
    return sizes[typeCode - TYPE_SINT8];
}

static ksbonjson_decodeStatus decodeAndReportUnsignedInteger(DecodeContext* const ctx, const uint8_t typeCode)
{
    const size_t size = unsignedIntTypeCodeToSize(typeCode);
    SHOULD_HAVE_ROOM_FOR_BYTES(size);
    return ctx->callbacks->onUnsignedInteger(decodeUnsignedInt(ctx, size), ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportSignedInteger(DecodeContext* const ctx, const uint8_t typeCode)
{
    const size_t size = signedIntTypeCodeToSize(typeCode);
    SHOULD_HAVE_ROOM_FOR_BYTES(size);
    return ctx->callbacks->onSignedInteger(decodeSignedInt(ctx, size), ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportFloat32(DecodeContext* const ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(4);
    return reportFloat(ctx, (double)decodeFloat32(ctx));
}

static ksbonjson_decodeStatus decodeAndReportFloat64(DecodeContext* const ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(8);
    return reportFloat(ctx, decodeFloat64(ctx));
}

/**
 * Decode a zigzag LEB128 value into a signed int64.
 */
static ksbonjson_decodeStatus decodeZigzagLEB128(DecodeContext* const ctx, int64_t* result)
{
    uint64_t zigzag = 0;
    unsigned shift = 0;

    for(;;)
    {
        SHOULD_HAVE_ROOM_FOR_BYTES(1);
        const uint8_t byte = *ctx->bufferCurrent++;
        zigzag |= ((uint64_t)(byte & 0x7f)) << shift;
        if(!(byte & 0x80))
        {
            break;
        }
        shift += 7;
        unlikely_if(shift >= 64)
        {
            return KSBONJSON_DECODE_VALUE_OUT_OF_RANGE;
        }
    }

    // Zigzag decode: unsigned to signed
    *result = (int64_t)((zigzag >> 1) ^ -(zigzag & 1));
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus decodeAndReportBigNumber(DecodeContext* const ctx)
{
    // Decode exponent (zigzag LEB128)
    int64_t exponent;
    PROPAGATE_ERROR(ctx, decodeZigzagLEB128(ctx, &exponent));

    // Decode signed_length (zigzag LEB128)
    int64_t signedLength;
    PROPAGATE_ERROR(ctx, decodeZigzagLEB128(ctx, &signedLength));

    int32_t sign;
    uint64_t significand;

    if(signedLength == 0)
    {
        // Zero significand
        sign = 1;
        significand = 0;
    }
    else
    {
        // Extract sign and byte count
        sign = (signedLength > 0) ? 1 : -1;
        const int64_t byteCount = (signedLength > 0) ? signedLength : -signedLength;

        // Validate byte count fits in uint64_t
        unlikely_if(byteCount > 8)
        {
            return KSBONJSON_DECODE_VALUE_OUT_OF_RANGE;
        }

        SHOULD_HAVE_ROOM_FOR_BYTES((size_t)byteCount);

        // Validate normalization: last byte (most significant) must be non-zero
        unlikely_if(ctx->bufferCurrent[byteCount - 1] == 0)
        {
            return KSBONJSON_DECODE_INVALID_DATA;
        }

        // Read magnitude as LE unsigned integer
        union number_bits bits = {.u64 = 0};
        memcpy(bits.b, ctx->bufferCurrent, (size_t)byteCount);
        bits.u64 = fromLittleEndian(bits.u64);
        ctx->bufferCurrent += byteCount;

        significand = bits.u64;
    }

    return ctx->callbacks->onBigNumber(ksbonjson_newBigNumber(sign, significand, (int32_t)exponent), ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportShortString(DecodeContext* const ctx, const uint8_t typeCode)
{
    const size_t length = (size_t)(typeCode - TYPE_STRING0);
    SHOULD_HAVE_ROOM_FOR_BYTES(length);
    const uint8_t* const begin = ctx->bufferCurrent;
    ctx->bufferCurrent += length;
    return ctx->callbacks->onString((const char*)begin, length, ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportLongString(DecodeContext* const ctx)
{
    // Long string: data terminated by 0xFF
    // Since UTF-8 never contains 0xFF, we scan for the delimiter.
    const uint8_t* const start = ctx->bufferCurrent;
    const uint8_t* const end = ctx->bufferEnd;

    const uint8_t* terminator = (const uint8_t*)memchr(start, 0xff, (size_t)(end - start));
    unlikely_if(terminator == NULL)
    {
        return KSBONJSON_DECODE_INCOMPLETE;
    }

    const size_t length = (size_t)(terminator - start);
    ctx->bufferCurrent = terminator + 1; // Skip past the terminating 0xFF

    return ctx->callbacks->onString((const char*)start, length, ctx->userData);
}

static ksbonjson_decodeStatus beginArray(DecodeContext* const ctx)
{
    unlikely_if(ctx->containerDepth >= KSBONJSON_MAX_CONTAINER_DEPTH)
    {
        return KSBONJSON_DECODE_CONTAINER_DEPTH_EXCEEDED;
    }

    ctx->containerDepth++;
    ctx->containers[ctx->containerDepth] = (ContainerState){0};

    return ctx->callbacks->onBeginArray(ctx->userData);
}

static ksbonjson_decodeStatus beginObject(DecodeContext* const ctx)
{
    unlikely_if(ctx->containerDepth >= KSBONJSON_MAX_CONTAINER_DEPTH)
    {
        return KSBONJSON_DECODE_CONTAINER_DEPTH_EXCEEDED;
    }

    ctx->containerDepth++;
    ctx->containers[ctx->containerDepth] = (ContainerState)
                                            {
                                                .isObject = true,
                                                .isExpectingName = true,
                                            };

    return ctx->callbacks->onBeginObject(ctx->userData);
}

static ksbonjson_decodeStatus endContainer(DecodeContext* const ctx)
{
    unlikely_if(ctx->containerDepth <= 0)
    {
        return KSBONJSON_DECODE_UNBALANCED_CONTAINERS;
    }

    ContainerState* const container = &ctx->containers[ctx->containerDepth];

    // Cannot close an object while expecting a value for a key
    unlikely_if(container->isObject && !container->isExpectingName)
    {
        return KSBONJSON_DECODE_EXPECTED_OBJECT_VALUE;
    }

    ctx->containerDepth--;

    PROPAGATE_ERROR(ctx, ctx->callbacks->onEndContainer(ctx->userData));

    // When returning to a parent object, we just completed a value (the container)
    if(ctx->containerDepth > 0)
    {
        ContainerState* const parent = &ctx->containers[ctx->containerDepth];
        if(parent->isObject)
        {
            parent->isExpectingName = true;
        }
    }

    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus decodeObjectName(DecodeContext* const ctx, const uint8_t typeCode)
{
    // Short strings: 0xd0-0xdf
    if(typeCode >= TYPE_STRING0 && typeCode <= TYPE_STRING15)
    {
        return decodeAndReportShortString(ctx, typeCode);
    }

    // Long string: 0xff
    if(typeCode == TYPE_STRINGL)
    {
        return decodeAndReportLongString(ctx);
    }

    return KSBONJSON_DECODE_EXPECTED_OBJECT_NAME;
}

static ksbonjson_decodeStatus decodeValue(DecodeContext* const ctx, const uint8_t typeCode)
{
    // Small integers: 0x00-0xc8 (value = type_code - 100)
    if(typeCode <= SMALLINT_MAX_TYPE_CODE)
    {
        const int64_t value = (int64_t)typeCode - SMALLINT_BIAS;
        return ctx->callbacks->onSignedInteger(value, ctx->userData);
    }

    // Reserved: 0xc9
    if(typeCode == TYPE_RESERVED_C9)
    {
        return KSBONJSON_DECODE_INVALID_DATA;
    }

    // Scalar types: 0xca-0xcf
    switch(typeCode)
    {
        case TYPE_BIG_NUMBER:
            return decodeAndReportBigNumber(ctx);
        case TYPE_FLOAT32:
            return decodeAndReportFloat32(ctx);
        case TYPE_FLOAT64:
            return decodeAndReportFloat64(ctx);
        case TYPE_NULL:
            return ctx->callbacks->onNull(ctx->userData);
        case TYPE_FALSE:
            return ctx->callbacks->onBoolean(false, ctx->userData);
        case TYPE_TRUE:
            return ctx->callbacks->onBoolean(true, ctx->userData);
        default:
            break;
    }

    // Short strings: 0xd0-0xdf
    if(typeCode >= TYPE_STRING0 && typeCode <= TYPE_STRING15)
    {
        return decodeAndReportShortString(ctx, typeCode);
    }

    // Unsigned integers: 0xe0-0xe3
    if(typeCode >= TYPE_UINT8 && typeCode <= TYPE_UINT64)
    {
        return decodeAndReportUnsignedInteger(ctx, typeCode);
    }

    // Signed integers: 0xe4-0xe7
    if(typeCode >= TYPE_SINT8 && typeCode <= TYPE_SINT64)
    {
        return decodeAndReportSignedInteger(ctx, typeCode);
    }

    // Containers
    switch(typeCode)
    {
        case TYPE_ARRAY:
            return beginArray(ctx);
        case TYPE_OBJECT:
            return beginObject(ctx);
        case TYPE_STRINGL:
            return decodeAndReportLongString(ctx);
        default:
            // Reserved: 0xe8-0xfb and TYPE_END (0xfe) handled by caller
            return KSBONJSON_DECODE_INVALID_DATA;
    }
}

static ksbonjson_decodeStatus decodeDocument(DecodeContext* const ctx)
{
    // Empty document is invalid
    unlikely_if(ctx->bufferCurrent >= ctx->bufferEnd)
    {
        return KSBONJSON_DECODE_EMPTY_DOCUMENT;
    }

    while(ctx->bufferCurrent < ctx->bufferEnd)
    {
        ContainerState* const container = &ctx->containers[ctx->containerDepth];

        const uint8_t typeCode = *ctx->bufferCurrent++;

        // Handle container end marker
        if(typeCode == TYPE_END)
        {
            PROPAGATE_ERROR(ctx, endContainer(ctx));

            // After completing the top-level value, stop decoding
            if(ctx->containerDepth == 0)
            {
                break;
            }
            continue;
        }

        if(container->isObject && container->isExpectingName)
        {
            PROPAGATE_ERROR(ctx, decodeObjectName(ctx, typeCode));
            container->isExpectingName = false;
        }
        else
        {
            PROPAGATE_ERROR(ctx, decodeValue(ctx, typeCode));

            // For objects, after decoding a value, expect a name next
            if(ctx->containerDepth > 0)
            {
                ContainerState* const currentContainer = &ctx->containers[ctx->containerDepth];
                if(currentContainer->isObject)
                {
                    currentContainer->isExpectingName = true;
                }
            }

            // After completing the top-level value (a primitive), stop decoding
            if(ctx->containerDepth == 0)
            {
                break;
            }
        }
    }

    unlikely_if(ctx->containerDepth > 0)
    {
        return KSBONJSON_DECODE_UNCLOSED_CONTAINERS;
    }

    // Check for trailing data
    unlikely_if(ctx->bufferCurrent < ctx->bufferEnd)
    {
        return KSBONJSON_DECODE_TRAILING_DATA;
    }

    return ctx->callbacks->onEndData(ctx->userData);
}


// ============================================================================
// API
// ============================================================================

ksbonjson_decodeStatus ksbonjson_decode(const uint8_t* const document,
                                        const size_t documentLength,
                                        const KSBONJSONDecodeCallbacks* const callbacks,
                                        void* const userData,
                                        size_t* const decodedOffset)
{
    DecodeContext ctx =
        {
            .bufferCurrent = document,
            .bufferEnd = document + documentLength,
            .callbacks = callbacks,
            .userData = userData,
        };

    const ksbonjson_decodeStatus result = decodeDocument(&ctx);
    *decodedOffset = (size_t)(ctx.bufferCurrent - document);
    return result;
}

const char* ksbonjson_describeDecodeStatus(const ksbonjson_decodeStatus status)
{
    switch(status)
    {
        case KSBONJSON_DECODE_OK:
            return "Successful completion";
        case KSBONJSON_DECODE_INCOMPLETE:
            return "Incomplete data (document was truncated?)";
        case KSBONJSON_DECODE_UNCLOSED_CONTAINERS:
            return "Not all containers have been closed yet (likely the document has been truncated)";
        case KSBONJSON_DECODE_CONTAINER_DEPTH_EXCEEDED:
            return "The document had too much container depth";
        case KSBONJSON_DECODE_UNBALANCED_CONTAINERS:
            return "Tried to close too many containers";
        case KSBONJSON_DECODE_EXPECTED_OBJECT_NAME:
            return "Expected to find a string for an object element name";
        case KSBONJSON_DECODE_EXPECTED_OBJECT_VALUE:
            return "Got an end container while expecting an object element value";
        case KSBONJSON_DECODE_COULD_NOT_PROCESS_DATA:
            return "A callback failed to process the passed in data";
        case KSBONJSON_DECODE_INVALID_DATA:
            return "Encountered invalid data";
        case KSBONJSON_DECODE_DUPLICATE_OBJECT_NAME:
            return "This name already exists in the current object";
        case KSBONJSON_DECODE_NUL_CHARACTER:
            return "A string value contained a NUL character";
        case KSBONJSON_DECODE_VALUE_OUT_OF_RANGE:
            return "The value is out of range and cannot be stored without data loss";
        case KSBONJSON_DECODE_EMPTY_DOCUMENT:
            return "The document is empty (zero bytes)";
        case KSBONJSON_DECODE_TRAILING_DATA:
            return "There is data after the end of the top-level value";
        default:
            return "(unknown status - was it a user-defined status code?)";
    }
}
