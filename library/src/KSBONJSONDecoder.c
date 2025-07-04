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

#include "KSBONJSONDecoder.h"
#include "KSBONJSONCommon.h"
#include <string.h> // For memcpy() and strnlen()
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
    uint8_t isChunkingString: 1;
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

#define SHOULD_NOT_CONTAIN_NUL_CHARS(STRING, LENGTH) \
    unlikely_if(strnlen((const char*)(STRING), LENGTH) != (LENGTH)) \
        return KSBONJSON_DECODE_NUL_CHARACTER


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
 * Get a length field's total byte count from its header.
 * Note: Undefined for header value 0x00.
 */
static size_t decodeLengthFieldTotalByteCount(uint8_t header)
{
#if HAS_BUILTIN(__builtin_ctz)
    return (size_t)__builtin_ctz(header) + 1;
#elif defined(_MSC_VER)
    unsigned long tz = 0;
    _BitScanForward(&tz, header);
    return tz + 1;
#else
    // Isolate lowest 1-bit
    header &= -header;

    // Calculate log2
    uint8_t result = (header & 0xAA) != 0;
    result |= ((header & 0xCC) != 0) << 1;
    result |= ((header & 0xF0) != 0) << 2;

    // Add 1
    return result + 1;
#endif
}

static ksbonjson_decodeStatus decodeLengthPayload(DecodeContext* const ctx, uint64_t *payloadBuffer)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(1);
    const uint8_t header = *ctx->bufferCurrent;
    unlikely_if(header == 0)
    {
        ctx->bufferCurrent++;
        SHOULD_HAVE_ROOM_FOR_BYTES(8);
        union number_bits bits;
        memcpy(bits.b, ctx->bufferCurrent, 8);
        ctx->bufferCurrent += 8;
        *payloadBuffer = fromLittleEndian(bits.u64);
        return KSBONJSON_DECODE_OK;
    }

    const size_t count = decodeLengthFieldTotalByteCount(header);
    SHOULD_HAVE_ROOM_FOR_BYTES(count);
    union number_bits bits = {0};
    memcpy(bits.b, ctx->bufferCurrent, count);
    ctx->bufferCurrent += count;
    *payloadBuffer = fromLittleEndian(bits.u64 >> count);
    return KSBONJSON_DECODE_OK;
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

static int8_t fillWithBit0(uint8_t value)
{
    return (int8_t)(value<<7) >> 7;
}

static uint64_t decodeUnsignedInt(DecodeContext* const ctx, const size_t size)
{
    return decodePrimitiveNumeric(ctx, size, 0).u64;
}

static int64_t decodeSignedInt(DecodeContext* const ctx, const size_t size)
{
    return decodePrimitiveNumeric(ctx, size, fillWithBit7(ctx->bufferCurrent[size-1])).i64;
}

static float decodeFloat16(DecodeContext* const ctx)
{
    union number_bits value = decodePrimitiveNumeric(ctx, 2, 0);
    value.u32 <<= 16;
    return value.f32;
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

static ksbonjson_decodeStatus decodeAndReportUnsignedInteger(DecodeContext* const ctx, const uint8_t typeCode)
{
    const size_t size = (size_t)(typeCode - TYPE_UINT8 + 1);
    SHOULD_HAVE_ROOM_FOR_BYTES(size);
    return ctx->callbacks->onUnsignedInteger(decodeUnsignedInt(ctx, size), ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportSignedInteger(DecodeContext* const ctx, const uint8_t typeCode)
{
    const size_t size = (size_t)(typeCode - TYPE_SINT8 + 1);
    SHOULD_HAVE_ROOM_FOR_BYTES(size);
    return ctx->callbacks->onSignedInteger(decodeSignedInt(ctx, size), ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportFloat16(DecodeContext* const ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(2);
    return reportFloat(ctx, (double)decodeFloat16(ctx));
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

static ksbonjson_decodeStatus decodeAndReportBigNumber(DecodeContext* const ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(1);
    const uint8_t header = *ctx->bufferCurrent++;

    //   Header Byte
    // ───────────────
    // S S S S S E E N
    // ╰─┴─┼─┴─╯ ╰─┤ ╰─> Significand sign (0 = positive, 1 = negative)
    //     │       ╰───> Exponent Length (0-3 bytes)
    //     ╰───────────> Significand Length (0-31 bytes)

    const int sign = fillWithBit0((uint8_t)header);
    const size_t exponentLength = (header >> 1) & 3;
    const size_t significandLength = header >> 3;

    unlikely_if(significandLength > 8)
    {
        return KSBONJSON_DECODE_VALUE_OUT_OF_RANGE;
    }
    unlikely_if(significandLength == 0)
    {
        unlikely_if(exponentLength != 0)
        {
            // Special BigNumber encodings: Inf or NaN
            return KSBONJSON_DECODE_INVALID_DATA;
        }
        return ctx->callbacks->onBigNumber(ksbonjson_newBigNumber(sign, 0, 0), ctx->userData);
    }

    SHOULD_HAVE_ROOM_FOR_BYTES(significandLength + exponentLength);
    const int32_t exponent = (int32_t)decodeSignedInt(ctx, exponentLength);
    const uint64_t significand = decodeUnsignedInt(ctx, significandLength);

    return ctx->callbacks->onBigNumber(ksbonjson_newBigNumber(sign, significand, exponent), ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportShortString(DecodeContext* const ctx, const uint8_t typeCode)
{
    const size_t length = (size_t)(typeCode - TYPE_STRING0);
    SHOULD_HAVE_ROOM_FOR_BYTES(length);
    const uint8_t* const begin = ctx->bufferCurrent;
    ctx->bufferCurrent += length;
    SHOULD_NOT_CONTAIN_NUL_CHARS(begin, length);
    return ctx->callbacks->onString((const char*)begin, length, ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportLongString(DecodeContext* const ctx)
{
    uint64_t lengthPayload;
    PROPAGATE_ERROR(ctx, decodeLengthPayload(ctx, &lengthPayload));
    uint64_t length = lengthPayload >> 1;
    bool moreChunksFollow = (bool)(lengthPayload&1);
    SHOULD_HAVE_ROOM_FOR_BYTES(length);

    const uint8_t* pos = ctx->bufferCurrent;
    ctx->bufferCurrent += length;
    SHOULD_NOT_CONTAIN_NUL_CHARS(pos, length);

    likely_if(!moreChunksFollow)
    {
        return ctx->callbacks->onString((const char*)pos, length, ctx->userData);
    }

    PROPAGATE_ERROR(ctx, ctx->callbacks->onStringChunk((const char*)pos, length, moreChunksFollow, ctx->userData));

    while(moreChunksFollow)
    {
        PROPAGATE_ERROR(ctx, decodeLengthPayload(ctx, &lengthPayload));
        length = lengthPayload >> 1;
        moreChunksFollow = (bool)(lengthPayload&1);
        SHOULD_HAVE_ROOM_FOR_BYTES(length);
        pos = ctx->bufferCurrent;
        ctx->bufferCurrent += length;
        SHOULD_NOT_CONTAIN_NUL_CHARS(pos, length);
        PROPAGATE_ERROR(ctx, ctx->callbacks->onStringChunk((const char*)pos, length, moreChunksFollow, ctx->userData));
    }

    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus beginArray(DecodeContext* const ctx)
{
    unlikely_if(ctx->containerDepth > KSBONJSON_MAX_CONTAINER_DEPTH)
    {
        return KSBONJSON_DECODE_CONTAINER_DEPTH_EXCEEDED;
    }

    ctx->containerDepth++;
    ctx->containers[ctx->containerDepth] = (ContainerState){0};

    return ctx->callbacks->onBeginArray(ctx->userData);
}

static ksbonjson_decodeStatus beginObject(DecodeContext* const ctx)
{
    unlikely_if(ctx->containerDepth > KSBONJSON_MAX_CONTAINER_DEPTH)
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
    unlikely_if((container->isObject & !container->isExpectingName))
    {
        return KSBONJSON_DECODE_EXPECTED_OBJECT_VALUE;
    }

    ctx->containerDepth--;
    return ctx->callbacks->onEndContainer(ctx->userData);
}

static ksbonjson_decodeStatus decodeObjectName(DecodeContext* const ctx, const uint8_t typeCode)
{
    switch(typeCode)
    {
        case TYPE_END:
            return endContainer(ctx);
        case TYPE_STRING:
            return decodeAndReportLongString(ctx);
        case TYPE_STRING0:  case TYPE_STRING1:  case TYPE_STRING2:  case TYPE_STRING3:
        case TYPE_STRING4:  case TYPE_STRING5:  case TYPE_STRING6:  case TYPE_STRING7:
        case TYPE_STRING8:  case TYPE_STRING9:  case TYPE_STRING10: case TYPE_STRING11:
        case TYPE_STRING12: case TYPE_STRING13: case TYPE_STRING14: case TYPE_STRING15:
            return decodeAndReportShortString(ctx, typeCode);
        default:
            return KSBONJSON_DECODE_EXPECTED_OBJECT_NAME;
    }
}

static ksbonjson_decodeStatus decodeValue(DecodeContext* const ctx, const uint8_t typeCode)
{
    switch(typeCode)
    {
        case TYPE_STRING:
            return decodeAndReportLongString(ctx);
        case TYPE_STRING0:  case TYPE_STRING1:  case TYPE_STRING2:  case TYPE_STRING3:
        case TYPE_STRING4:  case TYPE_STRING5:  case TYPE_STRING6:  case TYPE_STRING7:
        case TYPE_STRING8:  case TYPE_STRING9:  case TYPE_STRING10: case TYPE_STRING11:
        case TYPE_STRING12: case TYPE_STRING13: case TYPE_STRING14: case TYPE_STRING15:
            return decodeAndReportShortString(ctx, typeCode);
        case TYPE_UINT8:  case TYPE_UINT16: case TYPE_UINT24: case TYPE_UINT32:
        case TYPE_UINT40: case TYPE_UINT48: case TYPE_UINT56: case TYPE_UINT64:
            return decodeAndReportUnsignedInteger(ctx, typeCode);
        case TYPE_SINT8:  case TYPE_SINT16: case TYPE_SINT24: case TYPE_SINT32:
        case TYPE_SINT40: case TYPE_SINT48: case TYPE_SINT56: case TYPE_SINT64:
            return decodeAndReportSignedInteger(ctx, typeCode);
        case TYPE_FLOAT16:
            return decodeAndReportFloat16(ctx);
        case TYPE_FLOAT32:
            return decodeAndReportFloat32(ctx);
        case TYPE_FLOAT64:
            return decodeAndReportFloat64(ctx);
        case TYPE_BIG_NUMBER:
            return decodeAndReportBigNumber(ctx);
        case TYPE_ARRAY:
            return beginArray(ctx);
        case TYPE_OBJECT:
            return beginObject(ctx);
        case TYPE_END:
            return endContainer(ctx);
        case TYPE_FALSE:
            return ctx->callbacks->onBoolean(false, ctx->userData);
        case TYPE_TRUE:
            return ctx->callbacks->onBoolean(true, ctx->userData);
        case TYPE_NULL:
            return ctx->callbacks->onNull(ctx->userData);
        case TYPE_RESERVED_65: case TYPE_RESERVED_66: case TYPE_RESERVED_67:
        case TYPE_RESERVED_90: case TYPE_RESERVED_91: case TYPE_RESERVED_92:
        case TYPE_RESERVED_93: case TYPE_RESERVED_94: case TYPE_RESERVED_95:
        case TYPE_RESERVED_96: case TYPE_RESERVED_97: case TYPE_RESERVED_98:
            return KSBONJSON_DECODE_INVALID_DATA;
        default:
            return ctx->callbacks->onSignedInteger((int8_t)typeCode, ctx->userData);
    }
}

static ksbonjson_decodeStatus decodeDocument(DecodeContext* const ctx)
{
    static ksbonjson_decodeStatus (*decodeFuncs[2])(DecodeContext*, const uint8_t) =
    {
        decodeValue, decodeObjectName,
    };

    while(ctx->bufferCurrent < ctx->bufferEnd)
    {
        ContainerState* const container = &ctx->containers[ctx->containerDepth];
        const uint8_t typeCode = *ctx->bufferCurrent++;
        PROPAGATE_ERROR(ctx, decodeFuncs[container->isObject & container->isExpectingName](ctx, typeCode));
        container->isExpectingName = !container->isExpectingName;
    }

    unlikely_if(ctx->containerDepth > 0)
    {
        return KSBONJSON_DECODE_UNCLOSED_CONTAINERS;
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
        default:
            return "(unknown status - was it a user-defined status code?)";
    }
}
