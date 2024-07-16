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

#include <ksbonjson/KSBONJSONEncoder.h>
#include <stddef.h>


// ============================================================================
// Helpers
// ============================================================================

// Compiler hints for "if" statements
#define likely_if(x) if(__builtin_expect(x,1))
#define unlikely_if(x) if(__builtin_expect(x,0))

enum {
    TYPE_ARRAY = 0xeb,
    TYPE_OBJECT = 0xec,
    TYPE_END = 0xed,
    TYPE_FALSE = 0xee,
    TYPE_TRUE = 0xef,
    TYPE_NULL = 0xf0,
    TYPE_INT8 = 0xf1,
    TYPE_INT16 = 0xf2,
    TYPE_INT24 = 0xf3,
    TYPE_INT32 = 0xf4,
    TYPE_INT40 = 0xf5,
    TYPE_INT48 = 0xf6,
    TYPE_INT56 = 0xf7,
    TYPE_INT64 = 0xf8,
    TYPE_UINT64 = 0xf9,
    TYPE_BIGPOSITIVE = 0xfa,
    TYPE_BIGNEGATIVE = 0xfb,
    TYPE_FLOAT16 = 0xfc,
    TYPE_FLOAT32 = 0xfd,
    TYPE_FLOAT64 = 0xfe,
    TYPE_STRING = 0xff,
};

#define INTSMALL_MAX 234
#define INTSMALL_BIAS 117

union uint64_u
{
    uint64_t u64;
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

#define PROPAGATE_ERROR(CALL) \
    do \
    { \
        const ksbonjson_encodeStatus propagatedResult = CALL; \
        unlikely_if(propagatedResult != KSBONJSON_ENCODE_OK) \
        { \
            return propagatedResult; \
        } \
    } \
    while(0)

#define SHOULD_BE_IN_OBJECT() \
    unlikely_if(!container->isObject) \
    { \
        return KSBONJSON_ENCODE_NOT_IN_AN_OBJECT; \
    }

#define SHOULD_NOT_BE_EXPECTING_OBJECT_NAME() \
    unlikely_if(container->isObject \
    && container->isExpectingName) \
    { \
        return KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME; \
    }

#define SHOULD_NOT_BE_EXPECTING_OBJECT_VALUE() \
    unlikely_if(container->isObject \
    && !container->isExpectingName) \
    { \
        return KSBONJSON_ENCODE_EXPECTED_OBJECT_VALUE; \
    }

#define SHOULD_NOT_BE_CHUNKING_STRING() \
    unlikely_if(container->isChunkingString) \
    { \
        return KSBONJSON_ENCODE_CHUNKING_STRING; \
    }

#define SHOULD_NOT_BE_NULL(VALUE) \
    unlikely_if((VALUE) == NULL) \
    { \
        return KSBONJSON_ENCODE_NULL_POINTER; \
    }

static ksbonjson_encodeStatus addBytes(KSBONJSONEncodeContext* const ctx,
                                       const uint8_t* const data,
                                       const size_t length)
{
    return ctx->addEncodedData(data, length, ctx->userData);
}

static ksbonjson_encodeStatus addByte(KSBONJSONEncodeContext* const ctx, const uint8_t value)
{
    const uint8_t b = value;
    return addBytes(ctx, &b, 1);
}

static ksbonjson_encodeStatus beginContainer(KSBONJSONEncodeContext* const ctx,
                                             const uint8_t typeCode,
                                             const KSBONJSONContainerState containerState)
{
    KSBONJSONContainerState* const container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    container->isExpectingName = true;
    ctx->containerDepth++;
    ctx->containers[ctx->containerDepth] = containerState;
    return addByte(ctx, typeCode);
}


// ============================================================================
// API
// ============================================================================

void ksbonjson_beginEncode(KSBONJSONEncodeContext* const ctx,
                           const KSBONJSONAddEncodedDataFunc addBytesFunc,
                           void* const userData)
{
    *ctx = (KSBONJSONEncodeContext){0};
    ctx->addEncodedData = addBytesFunc;
    ctx->userData = userData;
}

ksbonjson_encodeStatus ksbonjson_endEncode(KSBONJSONEncodeContext* const ctx)
{
    unlikely_if(ctx->containerDepth > 0)
    {
        return KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN;
    }
    unlikely_if(ctx->containers[ctx->containerDepth].isChunkingString)
    {
        return KSBONJSON_ENCODE_CHUNKING_STRING;
    }
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_terminateDocument(KSBONJSONEncodeContext* const ctx)
{
    while(ctx->containerDepth > 0)
    {
        PROPAGATE_ERROR(ksbonjson_endContainer(ctx));
    }
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_addBoolean(KSBONJSONEncodeContext* const ctx, const bool value)
{
    KSBONJSONContainerState* const container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    container->isExpectingName = true;
    return addByte(ctx, value ? TYPE_TRUE : TYPE_FALSE);
}

ksbonjson_encodeStatus ksbonjson_addInteger(KSBONJSONEncodeContext* const ctx, const int64_t value)
{
    KSBONJSONContainerState* const container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    container->isExpectingName = true;
    if(value >= -INTSMALL_BIAS && value <= INTSMALL_MAX - INTSMALL_BIAS)
    {
        // Small Int
        return addByte(ctx, (uint8_t)(value + INTSMALL_BIAS));
    }
    if(value >= (-128 - INTSMALL_BIAS) && value <= (127 + INTSMALL_BIAS + 1))
    {
        // Int8
        uint8_t data[] =
        {
            TYPE_INT8,
            (uint8_t)(value + (value < 0 ? INTSMALL_BIAS : -INTSMALL_BIAS - 1)),
        };
        return addBytes(ctx, data, sizeof(data));
    }

    // Integers from 2 to 8 bytes
    int byteCount = 2;
    const int64_t endValue = value < 0 ? -1 : 0;
    for(int64_t v = value >> 15; v != endValue; v >>= 8)
    {
        byteCount++;
    }

    // Allocate 2 unions to give scratch space in front of the encoded u64
    union uint64_u u[2];
    u[1].u64 = (uint64_t)value;
#if KSBONJSON_IS_LITTLE_ENDIAN
    // The last byte of our scratch space will hold the type code
    u[0].b[7] = 0xf0 + byteCount;
    return addBytes(ctx, &u[0].b[7], byteCount + 1);
#else
    uint8_t data[byteCount + 1];
    data[0] = 0xf0 + byteCount; // Type code
    for(int i = 0; i < byteCount; i++)
    {
        data[byteCount - i] = u[1].b[i];
    }
    return addBytes(ctx, data, sizeof(data));
#endif
}

ksbonjson_encodeStatus ksbonjson_addFloat(KSBONJSONEncodeContext* const ctx, const double value)
{
    const int64_t asInt = (int64_t)value;
    unlikely_if((double)asInt == value)
    {
        return ksbonjson_addInteger(ctx, asInt);
    }

    KSBONJSONContainerState* const container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    // Allocate 2 unions to give scratch space in front of the encoded f64
    union float64_u f64[2];
    f64[1].f64 = value;

    // When all exponent bits are set, it signifies an infinite or NaN value
    unlikely_if((f64[1].u64 & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL)
    {
        // If the significand is 0, it's infinite
        if((f64[1].u64 & 0x000fffffffffffffULL) == 0)
        {
            return KSBONJSON_ENCODE_INF;
        }
        return KSBONJSON_ENCODE_NAN;
    }

    container->isExpectingName = true;

    // Allocate 2 unions to give scratch space in front of the encoded f32
    union float32_u f32[2];
    f32[1].f32 = value;
    if((double)f32[1].f32 == value)
    {
        // Use our scratch space to build a potential f16 encoding
        f32[0].f32 = value;
        f32[0].b[0] = 0;
        f32[0].b[1] = 0;
        if((double)f32[0].f32 == value)
        {
            f32[0].b[1] = TYPE_FLOAT16;
            return addBytes(ctx, &f32[0].b[1], 3);
        }
#if KSBONJSON_IS_LITTLE_ENDIAN
        // The last byte of our scratch space will hold the type code
        f32[0].b[3] = TYPE_FLOAT32;
        return addBytes(ctx, &f32[0].b[3], 5);
#else
        uint8_t data[] = {TYPE_FLOAT32, f32[1].b[3], f32[1].b[2], f32[1].b[1], f32[1].b[0]};
        return addBytes(ctx, data, sizeof(data));
#endif
    }

#if KSBONJSON_IS_LITTLE_ENDIAN
    // The last byte of our scratch space will hold the type code
    f64[0].b[7] = TYPE_FLOAT64;
    return addBytes(ctx, &f64[0].b[7], 9);
#else
    uint8_t data[] = {TYPE_FLOAT64, f64[1].b[7], f64[1].b[6], f64[1].b[5], f64[1].b[4], f64[1].b[3], f64[1].b[2], f64[1].b[1], f64[1].b[0]};
    return addBytes(ctx, data, sizeof(data));
#endif
}

ksbonjson_encodeStatus ksbonjson_addUInteger(KSBONJSONEncodeContext* const ctx, const uint64_t value)
{
    likely_if(value < 0x8000000000000000ULL)
    {
        return ksbonjson_addInteger(ctx, (int64_t)value);
    }

    KSBONJSONContainerState* const container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    container->isExpectingName = true;

    // Allocate 2 unions to give scratch space in front of the encoded u64
    union uint64_u u[2];
    u[1].u64 = value;
#if KSBONJSON_IS_LITTLE_ENDIAN
    // The last byte of our scratch space will hold the type code
    u[0].b[7] = TYPE_UINT64;
    return addBytes(ctx, &u[0].b[7], 9);
#else
    uint8_t data[] = {TYPE_UINT64, u[1].b[7], u[1].b[6], u[1].b[5], u[1].b[4], u[1].b[3], u[1].b[2], u[1].b[1], u[1].b[0]};
    return addBytes(ctx, data, sizeof(data));
#endif
}

ksbonjson_encodeStatus ksbonjson_addNull(KSBONJSONEncodeContext* const ctx)
{
    KSBONJSONContainerState* const container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    container->isExpectingName = true;
    return addByte(ctx, TYPE_NULL);
}

ksbonjson_encodeStatus ksbonjson_addString(KSBONJSONEncodeContext* const ctx,
                                           const char* const value,
                                           const size_t valueLength)
{
    KSBONJSONContainerState* const container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_CHUNKING_STRING();
    SHOULD_NOT_BE_NULL(value);

    container->isExpectingName = !container->isExpectingName;

    PROPAGATE_ERROR(addByte(ctx, TYPE_STRING));
    PROPAGATE_ERROR(addBytes(ctx, (uint8_t*)value, valueLength));
    return addByte(ctx, TYPE_STRING);
}

ksbonjson_encodeStatus ksbonjson_chunkString(KSBONJSONEncodeContext* const ctx,
                                             const char* const chunk,
                                             const size_t chunkLength,
                                             const bool isLastChunk)
{
    KSBONJSONContainerState* const container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_NULL(chunk);

    unlikely_if(!container->isChunkingString)
    {
        PROPAGATE_ERROR(addByte(ctx, TYPE_STRING));
    }
    PROPAGATE_ERROR(addBytes(ctx, (uint8_t*)chunk, chunkLength));

    likely_if(!isLastChunk)
    {
        container->isChunkingString = true;
        return KSBONJSON_ENCODE_OK;
    }

    container->isChunkingString = false;
    container->isExpectingName = !container->isExpectingName;
    return addByte(ctx, TYPE_STRING);
}

ksbonjson_encodeStatus ksbonjson_addBONJSONDocument(KSBONJSONEncodeContext* const ctx,
                                                    const uint8_t* const bonjsonDocument,
                                                    const size_t documentLength)
{
    KSBONJSONContainerState* const container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    container->isExpectingName = true;
    return addBytes(ctx, bonjsonDocument, documentLength);
}

ksbonjson_encodeStatus ksbonjson_beginObject(KSBONJSONEncodeContext* const ctx)
{
    return beginContainer(ctx, TYPE_OBJECT, (KSBONJSONContainerState)
    {
        .isObject = true,
        .isExpectingName = true,
    });
}

ksbonjson_encodeStatus ksbonjson_beginArray(KSBONJSONEncodeContext* const ctx)
{
    return beginContainer(ctx, TYPE_ARRAY, (KSBONJSONContainerState){0});
}

ksbonjson_encodeStatus ksbonjson_endContainer(KSBONJSONEncodeContext* const ctx)
{
    KSBONJSONContainerState* const container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_VALUE();
    SHOULD_NOT_BE_CHUNKING_STRING();
    unlikely_if(ctx->containerDepth <= 0)
    {
        return KSBONJSON_ENCODE_CLOSED_TOO_MANY_CONTAINERS;
    }

    ctx->containerDepth--;
    return addByte(ctx, TYPE_END);
}

const char* ksbonjson_encodeStatusDescription(const ksbonjson_encodeStatus status)
{
    switch (status)
    {
        case KSBONJSON_ENCODE_OK:
            return "Successful completion";
        case KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME:
            return "Expected an object element name, but got a non-string";
        case KSBONJSON_ENCODE_EXPECTED_OBJECT_VALUE:
            return "Attempted to close an object while it's expecting a value for the current name";
        case KSBONJSON_ENCODE_CHUNKING_STRING:
            return "Attempted to add a discrete value while chunking a string";
        case KSBONJSON_ENCODE_NULL_POINTER:
            return "Passed in a NULL pointer";
        case KSBONJSON_ENCODE_CLOSED_TOO_MANY_CONTAINERS:
            return "Attempted to close more containers than there actually are";
        case KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN:
            return "Attempted to end the encoding while there are still containers open";
        case KSBONJSON_ENCODE_NAN:
            return "Attempted to encode a NaN value";
        case KSBONJSON_ENCODE_INF:
            return "Attempted to encode an infinite value";
        case KSBONJSON_ENCODE_COULD_NOT_ADD_DATA:
            return "addBytes() failed to process the passed in data";
        default:
            return "(unknown status - was it a user-defined status code?)";
    }
}
