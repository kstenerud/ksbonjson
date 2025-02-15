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

#include <ksbonjson/KSBONJSONDecoder.h>

#include <string.h>


// ============================================================================
// Helpers
// ============================================================================

// Compiler hints for "if" statements
#define likely_if(x) if(__builtin_expect(x,1))
#define unlikely_if(x) if(__builtin_expect(x,0))

enum
{
    TYPE_ARRAY = 0x91,
    TYPE_OBJECT = 0x92,
    TYPE_END = 0x93,
    TYPE_FALSE = 0x94,
    TYPE_TRUE = 0x95,
    TYPE_NULL = 0x96,
    TYPE_UINT8 = 0x70,
    TYPE_UINT16 = 0x71,
    TYPE_UINT24 = 0x72,
    TYPE_UINT32 = 0x73,
    TYPE_UINT40 = 0x74,
    TYPE_UINT48 = 0x75,
    TYPE_UINT56 = 0x76,
    TYPE_UINT64 = 0x77,
    TYPE_SINT8 = 0x78,
    TYPE_SINT16 = 0x79,
    TYPE_SINT24 = 0x7a,
    TYPE_SINT32 = 0x7b,
    TYPE_SINT40 = 0x7c,
    TYPE_SINT48 = 0x7d,
    TYPE_SINT56 = 0x7e,
    TYPE_SINT64 = 0x7f,
    TYPE_BIGPOSITIVE = 0x6e,
    TYPE_BIGNEGATIVE = 0x6f,
    TYPE_FLOAT16 = 0x6b,
    TYPE_FLOAT32 = 0x6c,
    TYPE_FLOAT64 = 0x6d,
    TYPE_STRING = 0x90,
    TYPE_STRING0 = 0x80,
    TYPE_STRING1 = 0x81,
    TYPE_STRING2 = 0x82,
    TYPE_STRING3 = 0x83,
    TYPE_STRING4 = 0x84,
    TYPE_STRING5 = 0x85,
    TYPE_STRING6 = 0x86,
    TYPE_STRING7 = 0x87,
    TYPE_STRING8 = 0x88,
    TYPE_STRING9 = 0x89,
    TYPE_STRING10 = 0x8a,
    TYPE_STRING11 = 0x8b,
    TYPE_STRING12 = 0x8c,
    TYPE_STRING13 = 0x8d,
    TYPE_STRING14 = 0x8e,
    TYPE_STRING15 = 0x8f,
};

enum
{
    STRING_TERMINATOR = 0xff,
    INTSMALL_NEGATIVE_EDGE = (unsigned char)-105,
    INTSMALL_POSITIVE_EDGE = 106,
};

union uint64_u
{
    uint64_t u64;
    uint8_t b[8];
};

union int64_u
{
    int64_t i64;
    uint8_t b[8];
};

union float32_u
{
    float f32;
    uint32_t u32;
    uint8_t b[4];
};

union float64_u
{
    double f64;
    uint64_t u64;
    uint8_t b[8];
};

// ============================================================================
// Implementation
// ============================================================================

typedef struct
{
    uint8_t isObject: 1;
    uint8_t isExpectingName: 1;
    uint8_t isChunkingString: 1;
} ContainerState;

typedef struct
{
    int containerDepth;
    ContainerState containers[KSBONJSON_MAX_CONTAINER_DEPTH];
    const uint8_t* const bufferStart;
    const uint8_t* bufferCurrent;
    const uint8_t* const bufferEnd;
    const KSBONJSONDecodeCallbacks* const callbacks;
    void* const userData;
} DecodeContext;

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
    do \
    { \
        unlikely_if(ctx->bufferCurrent + (BYTE_COUNT) > ctx->bufferEnd) \
        { \
            return KSBONJSON_DECODE_INCOMPLETE; \
        } \
    } \
    while(0)

/**
 * Decode up to 64 bits of ULEB128 data.
 */
static ksbonjson_decodeStatus decodeUleb128(DecodeContext* const ctx, uint64_t* const valuePtr)
{
    uint64_t nextByte = 0;
    uint64_t value = 0;
    int shift = 0;
    do
    {
        SHOULD_HAVE_ROOM_FOR_BYTES(1);
        nextByte = *ctx->bufferCurrent++;
        const uint64_t nextSegment = (nextByte&0x7f);
        unlikely_if(shift > 58)
        {
            unlikely_if(shift > 63)
            {
                return KSBONJSON_DECODE_TOO_BIG;
            }
            unlikely_if((nextSegment<<shift)>>shift != nextSegment)
            {
                return KSBONJSON_DECODE_TOO_BIG;
            }
        }
        value |= nextSegment << shift;
        shift += 7;
    }
    while((nextByte & 0x80) != 0);

    *valuePtr = value;
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus decodeBigNumber(DecodeContext* const ctx,
                                              uint64_t* const pSignificand, 
                                              int* const pExponent)
{
    uint64_t header = 0;
    PROPAGATE_ERROR(ctx, decodeUleb128(ctx, &header));
    const int exponentLength = header & 3;
    const uint64_t significandLength = header >> 2;

    unlikely_if(significandLength > 8)
    {
        return KSBONJSON_DECODE_TOO_BIG;
    }
    SHOULD_HAVE_ROOM_FOR_BYTES(significandLength + exponentLength);

    uint64_t significand = 0;
    const uint8_t* buffer = ctx->bufferCurrent;
    for(int i = (int)significandLength - 1; i >= 0; i--)
    {
        significand <<= 8;
        significand |= buffer[i];
    }
    ctx->bufferCurrent += significandLength;

    uint64_t exponent = 0;
    buffer = ctx->bufferCurrent;
    for(int i = exponentLength - 1; i >= 0; i--)
    {
        exponent <<= 8;
        exponent |= buffer[i];
    }
    ctx->bufferCurrent += exponentLength;

    *pSignificand = significand;
    *pExponent = exponent;
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus decodeAndReportUnsignedInteger(DecodeContext* const ctx, const int size)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(size);
    const uint8_t* const buf = ctx->bufferCurrent;
    ctx->bufferCurrent += size;
    union uint64_u u = {.u64 = 0};
#if KSBONJSON_IS_LITTLE_ENDIAN
    memcpy(u.b, buf, size);
#else
    union uint64_u u = {.u64 = buf[0]};
    for(int i = 0; i < size; i++)
    {
        u.b[i] = buf[(size-1) - i];
    }
#endif
    return ctx->callbacks->onUnsignedInteger(u.u64, ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportSignedInteger(DecodeContext* const ctx, const int size)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(size);
    const uint8_t* const buf = ctx->bufferCurrent;
    ctx->bufferCurrent += size;
    // Use the highest byte to sign-extend init the int64
    union int64_u u = {.i64 = (int8_t)buf[size-1]};
#if KSBONJSON_IS_LITTLE_ENDIAN
    memcpy(u.b, buf, size);
#else
    // Use the highest byte to sign-extend init the int64
    union int64_u u = {.i64 = (int8_t)buf[0]};
    for(int i = 0; i < size; i++)
    {
        u.b[i] = buf[(size-1) - i];
    }
#endif
    return ctx->callbacks->onSignedInteger(u.i64, ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportFloat16(DecodeContext* const ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(2);
    const uint8_t* const buf = ctx->bufferCurrent;
    ctx->bufferCurrent += 2;

#if KSBONJSON_IS_LITTLE_ENDIAN
    union float32_u u = {.b = {0, 0, buf[0], buf[1]}};
    return ctx->callbacks->onFloat(u.f32, ctx->userData);
#else
    union float32_u u = {.b = {buf[1], buf[0], 0, 0}};
    return ctx->callbacks->onFloat(u.f32, ctx->userData);
#endif
}

static ksbonjson_decodeStatus decodeAndReportFloat32(DecodeContext* const ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(4);
    const uint8_t* const buf = ctx->bufferCurrent;
    ctx->bufferCurrent += 4;

#if KSBONJSON_IS_LITTLE_ENDIAN
    float value;
    memcpy(&value, buf, sizeof(value));
    return ctx->callbacks->onFloat(value, ctx->userData);
#else
    union float32_u u = {.b = {buf[3], buf[2], buf[1], buf[0]}};
    return ctx->callbacks->onFloat(u.f32, ctx->userData);
#endif
}

static ksbonjson_decodeStatus decodeAndReportFloat64(DecodeContext* const ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(8);
    const uint8_t* const buf = ctx->bufferCurrent;
    ctx->bufferCurrent += 8;

#if KSBONJSON_IS_LITTLE_ENDIAN
    double value;
    memcpy(&value, buf, sizeof(value));
    return ctx->callbacks->onFloat(value, ctx->userData);
#else
    union float64_u u = {.b = {buf[7], buf[6], buf[5], buf[4], buf[3], buf[2], buf[1], buf[0]}};
    return ctx->callbacks->onFloat(u.f64, ctx->userData);
#endif
}

static ksbonjson_decodeStatus decodeAndReportPositiveBigNumber(DecodeContext* const ctx)
{
    uint64_t significand = 0;
    int exponent = 0;
    PROPAGATE_ERROR(ctx, decodeBigNumber(ctx, &significand, &exponent));
    // TODO: handle exponent
    return ctx->callbacks->onUnsignedInteger(significand, ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportNegativeBigNumber(DecodeContext* const ctx)
{
    uint64_t significand = 0;
    int exponent = 0;
    PROPAGATE_ERROR(ctx, decodeBigNumber(ctx, &significand, &exponent));
    // TODO: handle exponent
    unlikely_if(significand > 0x8000000000000000ULL)
    {
        return KSBONJSON_DECODE_TOO_BIG;
    }
    return ctx->callbacks->onSignedInteger(significand, ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportString(DecodeContext* const ctx)
{
    const uint8_t* pos = ctx->bufferCurrent;
    const char* const begin = (const char*)pos;
    const uint8_t* const end = ctx->bufferEnd;

    for(; pos < end; pos++)
    {
        if(*pos == STRING_TERMINATOR)
        {
            const size_t length = pos - ctx->bufferCurrent;
            ctx->bufferCurrent += length + 1;
            return ctx->callbacks->onString(begin, length, ctx->userData);
        }
    }

    return KSBONJSON_DECODE_INCOMPLETE;
}

static ksbonjson_decodeStatus decodeAndReportSmallString(DecodeContext* const ctx, const uint8_t typeCode)
{
    const char* const begin = (const char*)ctx->bufferCurrent;
    const size_t length = typeCode - TYPE_STRING0;

    if(begin + length > (const char*)ctx->bufferEnd)
    {
        return KSBONJSON_DECODE_INCOMPLETE;
    }

    ctx->bufferCurrent += length;
    return ctx->callbacks->onString(begin, length, ctx->userData);
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

    ctx->containerDepth--;
    PROPAGATE_ERROR(ctx, ctx->callbacks->onEndContainer(ctx->userData));

    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus decodeObjectName(DecodeContext* const ctx, uint8_t typeCode)
{
    switch(typeCode)
    {
        case TYPE_END:
            PROPAGATE_ERROR(ctx, endContainer(ctx));
            break;
        case TYPE_STRING:
            PROPAGATE_ERROR(ctx, decodeAndReportString(ctx));
            break;
        case TYPE_STRING0:
        case TYPE_STRING1:
        case TYPE_STRING2:
        case TYPE_STRING3:
        case TYPE_STRING4:
        case TYPE_STRING5:
        case TYPE_STRING6:
        case TYPE_STRING7:
        case TYPE_STRING8:
        case TYPE_STRING9:
        case TYPE_STRING10:
        case TYPE_STRING11:
        case TYPE_STRING12:
        case TYPE_STRING13:
        case TYPE_STRING14:
        case TYPE_STRING15:
            PROPAGATE_ERROR(ctx, decodeAndReportSmallString(ctx, typeCode));
            break;
        default:
            return KSBONJSON_DECODE_EXPECTED_OBJECT_NAME;
    }
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus decodeValue(DecodeContext* const ctx, uint8_t typeCode)
{
    switch(typeCode)
    {
        case TYPE_STRING:
            PROPAGATE_ERROR(ctx, decodeAndReportString(ctx));
            break;
        case TYPE_STRING0:
        case TYPE_STRING1:
        case TYPE_STRING2:
        case TYPE_STRING3:
        case TYPE_STRING4:
        case TYPE_STRING5:
        case TYPE_STRING6:
        case TYPE_STRING7:
        case TYPE_STRING8:
        case TYPE_STRING9:
        case TYPE_STRING10:
        case TYPE_STRING11:
        case TYPE_STRING12:
        case TYPE_STRING13:
        case TYPE_STRING14:
        case TYPE_STRING15:
            PROPAGATE_ERROR(ctx, decodeAndReportSmallString(ctx, typeCode));
            break;
        case TYPE_UINT8:
        case TYPE_UINT16:
        case TYPE_UINT24:
        case TYPE_UINT32:
        case TYPE_UINT40:
        case TYPE_UINT48:
        case TYPE_UINT56:
        case TYPE_UINT64:
            PROPAGATE_ERROR(ctx, decodeAndReportUnsignedInteger(ctx, typeCode - TYPE_UINT8 + 1));
            break;
        case TYPE_SINT8:
        case TYPE_SINT16:
        case TYPE_SINT24:
        case TYPE_SINT32:
        case TYPE_SINT40:
        case TYPE_SINT48:
        case TYPE_SINT56:
        case TYPE_SINT64:
            PROPAGATE_ERROR(ctx, decodeAndReportSignedInteger(ctx, typeCode - TYPE_SINT8 + 1));
            break;
        case TYPE_FLOAT16:
            PROPAGATE_ERROR(ctx, decodeAndReportFloat16(ctx));
            break;
        case TYPE_FLOAT32:
            PROPAGATE_ERROR(ctx, decodeAndReportFloat32(ctx));
            break;
        case TYPE_FLOAT64:
            PROPAGATE_ERROR(ctx, decodeAndReportFloat64(ctx));
            break;
        case TYPE_BIGPOSITIVE:
            PROPAGATE_ERROR(ctx, decodeAndReportPositiveBigNumber(ctx));
            break;
        case TYPE_BIGNEGATIVE:
            PROPAGATE_ERROR(ctx, decodeAndReportNegativeBigNumber(ctx));
            break;
        case TYPE_ARRAY:
            PROPAGATE_ERROR(ctx, beginArray(ctx));
            break;
        case TYPE_OBJECT:
            PROPAGATE_ERROR(ctx, beginObject(ctx));
            break;
        case TYPE_END:
            PROPAGATE_ERROR(ctx, endContainer(ctx));
            break;
        case TYPE_FALSE:
            PROPAGATE_ERROR(ctx, ctx->callbacks->onBoolean(false, ctx->userData));
            break;
        case TYPE_TRUE:
            PROPAGATE_ERROR(ctx, ctx->callbacks->onBoolean(true, ctx->userData));
            break;
        case TYPE_NULL:
            PROPAGATE_ERROR(ctx, ctx->callbacks->onNull(ctx->userData));
            break;
        default:
            PROPAGATE_ERROR(ctx, ctx->callbacks->onSignedInteger((int8_t)typeCode, ctx->userData));
            break;
    }
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus decode(DecodeContext* const ctx)
{
    while(ctx->bufferCurrent < ctx->bufferEnd)
    {
        ContainerState* const container = &ctx->containers[ctx->containerDepth];
        const uint8_t typeCode = *ctx->bufferCurrent++;

        unlikely_if(typeCode == TYPE_END && container->isObject && !container->isExpectingName)
        {
            return KSBONJSON_DECODE_EXPECTED_OBJECT_VALUE;
        }

        if(container->isObject && container->isExpectingName)
        {
            PROPAGATE_ERROR(ctx, decodeObjectName(ctx, typeCode));
        }
        else
        {
            PROPAGATE_ERROR(ctx, decodeValue(ctx, typeCode));
        }
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
        .bufferStart = document,
        .bufferCurrent = document,
        .bufferEnd = document + documentLength,
        .callbacks = callbacks,
        .userData = userData,
    };

    const ksbonjson_decodeStatus result = decode(&ctx);
    *decodedOffset = ctx.bufferCurrent - ctx.bufferStart;
    return result;
}

const char* ksbonjson_decodeStatusDescription(const ksbonjson_decodeStatus status)
{
    switch(status)
    {
        case KSBONJSON_DECODE_OK:
            return "Successful completion";
        case KSBONJSON_DECODE_INCOMPLETE:
            return "Incomplete data (document was truncated?)";
        case KSBONJSON_DECODE_TOO_BIG:
            return "Decoded a value that was too big or long";
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
        default:
            return "(unknown status - was it a user-defined status code?)";
    }
}
