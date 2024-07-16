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

#define MAX_INT16  0x7fffLL
#define MIN_INT16 -0x8000LL
#define MAX_INT24  0x7fffffLL
#define MIN_INT24 -0x800000LL
#define MAX_INT32  0x7fffffffLL
#define MIN_INT32 -0x80000000LL
#define MAX_INT40  0x7fffffffffLL
#define MIN_INT40 -0x8000000000LL
#define MAX_INT48  0x7fffffffffffLL
#define MIN_INT48 -0x800000000000LL
#define MAX_INT56  0x7fffffffffffffLL
#define MIN_INT56 -0x80000000000000LL
#define MAX_INT64  0x7fffffffffffffffLL
#define MIN_INT64 -0x8000000000000000LL

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
        ksbonjson_encodeStatus propagatedResult = CALL; \
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
                                             const uint8_t* data,
                                             size_t length)
{
    return ctx->addEncodedData(data, length, ctx->userData);
}

static ksbonjson_encodeStatus addByte(KSBONJSONEncodeContext* const ctx, uint8_t typeCode)
{
    return addBytes(ctx, &typeCode, 1);
}

static ksbonjson_encodeStatus encodeStringChunk(KSBONJSONEncodeContext* const ctx,
                                                const char* restrict const data,
                                                size_t length,
                                                bool isLastChunk) {
    PROPAGATE_ERROR(addBytes(ctx, (uint8_t*)data, length));
    if(isLastChunk)
    {
        return addByte(ctx, TYPE_STRING);
    }
    return KSBONJSON_ENCODE_OK;
}

static ksbonjson_encodeStatus encodeString(KSBONJSONEncodeContext* const ctx,
                                           const char* restrict const value,
                                           size_t valueLength)
{
    PROPAGATE_ERROR(addByte(ctx, TYPE_STRING));
    PROPAGATE_ERROR(addBytes(ctx, (uint8_t*)value, valueLength));
    return addByte(ctx, TYPE_STRING);
}

static ksbonjson_encodeStatus encodeIntSmall(KSBONJSONEncodeContext* const ctx, int value)
{
    return addByte(ctx, (uint8_t)(value + INTSMALL_BIAS));
}

static ksbonjson_encodeStatus encodeInt8(KSBONJSONEncodeContext* const ctx, int value)
{
    uint8_t data[1+1];
    data[0] = TYPE_INT8;

    if(value < 0)
    {
        data[1] = (uint8_t)(value + INTSMALL_BIAS);
    }
    else
    {
        data[1] = (uint8_t)(value - (INTSMALL_BIAS+1));
    }

    return addBytes(ctx, data, sizeof(data));
}

static ksbonjson_encodeStatus encodeInt16(KSBONJSONEncodeContext* const ctx, int16_t value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
    union uint64_u u[2];
    u[1].u64 = value;
    u[0].b[7] = TYPE_INT16;
    return addBytes(ctx, &u[0].b[7], 3);
#else
    union uint64_u u = {.u64 = value};
    uint8_t data[] = {TYPE_INT16, u.b[1], u.b[0]};
    return addBytes(ctx, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeInt24(KSBONJSONEncodeContext* const ctx, int32_t value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
    union uint64_u u[2];
    u[1].u64 = value;
    u[0].b[7] = TYPE_INT24;
    return addBytes(ctx, &u[0].b[7], 4);
#else
    union uint64_u u = {.u64 = value};
    uint8_t data[] = {TYPE_INT24, u.b[2], u.b[1], u.b[0]};
    return addBytes(ctx, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeInt32(KSBONJSONEncodeContext* const ctx, int32_t value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
    union uint64_u u[2];
    u[1].u64 = value;
    u[0].b[7] = TYPE_INT32;
    return addBytes(ctx, &u[0].b[7], 5);
#else
    union uint64_u u = {.u64 = value};
    uint8_t data[] = {TYPE_INT32, u.b[3], u.b[2], u.b[1], u.b[0]};
    return addBytes(ctx, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeInt40(KSBONJSONEncodeContext* const ctx, int64_t value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
    union uint64_u u[2];
    u[1].u64 = value;
    u[0].b[7] = TYPE_INT40;
    return addBytes(ctx, &u[0].b[7], 6);
#else
    union uint64_u u = {.u64 = value};
    uint8_t data[] = {TYPE_INT40, u.b[4], u.b[3], u.b[2], u.b[1], u.b[0]};
    return addBytes(ctx, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeInt48(KSBONJSONEncodeContext* const ctx, int64_t value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
    union uint64_u u[2];
    u[1].u64 = value;
    u[0].b[7] = TYPE_INT48;
    return addBytes(ctx, &u[0].b[7], 7);
#else
    union uint64_u u = {.u64 = value};
    uint8_t data[] = {TYPE_INT48, u.b[5], u.b[4], u.b[3], u.b[2], u.b[1], u.b[0]};
    return addBytes(ctx, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeInt56(KSBONJSONEncodeContext* const ctx, int64_t value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
    union uint64_u u[2];
    u[1].u64 = value;
    u[0].b[7] = TYPE_INT56;
    return addBytes(ctx, &u[0].b[7], 8);
#else
    union uint64_u u = {.u64 = value};
    uint8_t data[] = {TYPE_INT56, u.b[6], u.b[5], u.b[4], u.b[3], u.b[2], u.b[1], u.b[0]};
    return addBytes(ctx, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeInt64(KSBONJSONEncodeContext* const ctx, int64_t value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
    union uint64_u u[2];
    u[1].u64 = value;
    u[0].b[7] = TYPE_INT64;
    return addBytes(ctx, &u[0].b[7], 9);
#else
    union uint64_u u = {.u64 = (uint64_t)value};
    uint8_t data[] = {TYPE_INT64, u.b[7], u.b[6], u.b[5], u.b[4], u.b[3], u.b[2], u.b[1], u.b[0]};
    return addBytes(ctx, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeUInt64(KSBONJSONEncodeContext* const ctx, uint64_t value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
    union uint64_u u[2];
    u[1].u64 = value;
    u[0].b[7] = TYPE_UINT64;
    return addBytes(ctx, &u[0].b[7], 9);
#else
    union uint64_u u = {.u64 = value};
    uint8_t data[] = {TYPE_UINT64, u.b[7], u.b[6], u.b[5], u.b[4], u.b[3], u.b[2], u.b[1], u.b[0]};
    return addBytes(ctx, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeFloat32(KSBONJSONEncodeContext* const ctx, float value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
    union float32_u u[2];
    u[1].f32 = value;
    u[0].b[3] = TYPE_FLOAT32;
    return addBytes(ctx, &u[0].b[3], 5);
#else
    union float32_u u = {.f32 = value};
    uint8_t data[] = {TYPE_FLOAT32, u.b[3], u.b[2], u.b[1], u.b[0]};
    return addBytes(ctx, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeFloat64(KSBONJSONEncodeContext* const ctx, double value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
    union float64_u u[2];
    u[1].f64 = value;
    u[0].b[7] = TYPE_FLOAT64;
    return addBytes(ctx, &u[0].b[7], 9);
#else
    union float64_u u = {.f64 = value};
    uint8_t data[] = {TYPE_FLOAT64, u.b[7], u.b[6], u.b[5], u.b[4], u.b[3], u.b[2], u.b[1], u.b[0]};
    return addBytes(ctx, data, sizeof(data));
#endif
}


// ============================================================================
// API
// ============================================================================

void ksbonjson_beginEncode(KSBONJSONEncodeContext* const ctx,
                           KSBONJSONAddEncodedDataFunc addBytesFunc,
                           void* const userData)
{
    *ctx = (KSBONJSONEncodeContext){0};
    ctx->addEncodedData = addBytesFunc;
    ctx->userData = userData;
}

ksbonjson_encodeStatus ksbonjson_endEncode(KSBONJSONEncodeContext* ctx)
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

ksbonjson_encodeStatus ksbonjson_terminateDocument(KSBONJSONEncodeContext* ctx)
{
    while(ctx->containerDepth > 0)
    {
        PROPAGATE_ERROR(ksbonjson_endContainer(ctx));
    }
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_addBoolean(KSBONJSONEncodeContext* ctx, bool value)
{
    KSBONJSONContainerState* container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    container->isExpectingName = true;
    return addByte(ctx, value ? TYPE_TRUE : TYPE_FALSE);
}

ksbonjson_encodeStatus ksbonjson_addInteger(KSBONJSONEncodeContext* ctx, int64_t value)
{
    KSBONJSONContainerState* container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    container->isExpectingName = true;
    if(value >= -INTSMALL_BIAS && value <= INTSMALL_MAX - INTSMALL_BIAS)
    {
        return encodeIntSmall(ctx, value);
    }
    if(value >= (-128 - INTSMALL_BIAS) && value <= (127 + INTSMALL_BIAS + 1))
    {
        return encodeInt8(ctx, value);
    }
    if(value >= MIN_INT16 && value <= MAX_INT16)
    {
        return encodeInt16 (ctx, value);
    }
    if(value >= MIN_INT24 && value <= MAX_INT24)
    {
        return encodeInt24 (ctx, value);
    }
    if(value >= MIN_INT32 && value <= MAX_INT32)
    {
        return encodeInt32 (ctx, value);
    }
    if(value >= MIN_INT40 && value <= MAX_INT40)
    {
        return encodeInt40 (ctx, value);
    }
    if(value >= MIN_INT48 && value <= MAX_INT48)
    {
        return encodeInt48 (ctx, value);
    }
    if(value >= MIN_INT56 && value <= MAX_INT56)
    {
        return encodeInt56 (ctx, value);
    }
    return encodeInt64(ctx, value);
}

ksbonjson_encodeStatus ksbonjson_addFloat(KSBONJSONEncodeContext* ctx, double value)
{
    int64_t asInt = (int64_t)value;
    unlikely_if((double)asInt == value)
    {
        return ksbonjson_addInteger(ctx, asInt);
    }

    KSBONJSONContainerState* container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    container->isExpectingName = true;

    {
        union float32_u u = {.f32 = (float)value};
#if KSBONJSON_IS_LITTLE_ENDIAN
        u.b[0] = 0;
        u.b[1] = 0;
        if((double)u.f32 == value)
        {
            u.b[1] = TYPE_FLOAT16;
            return addBytes(ctx, &u.b[1], 3);
        }
#else
        u.b[3] = 0;
        u.b[4] = 0;
        if((double)u.f32 == value)
        {
            uint8_t data[] = {TYPE_FLOAT16, u.b[1], u.b[0]};
            return addBytes(ctx, data, sizeof(data));
        }
#endif
    }

    {
        float toValue = (float)value;
        if((double)toValue == value)
        {
            return encodeFloat32(ctx, toValue);
        }
    }

    return encodeFloat64(ctx, value);
}

ksbonjson_encodeStatus ksbonjson_addUInteger(KSBONJSONEncodeContext* ctx, uint64_t value)
{
    if(value < 0x8000000000000000ULL)
    {
        return ksbonjson_addInteger(ctx, (int64_t)value);
    }

    KSBONJSONContainerState* container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    container->isExpectingName = true;
    return encodeUInt64(ctx, value);
}

ksbonjson_encodeStatus ksbonjson_addNull(KSBONJSONEncodeContext* ctx)
{
    KSBONJSONContainerState* container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    container->isExpectingName = true;
    return addByte(ctx, TYPE_NULL);
}

ksbonjson_encodeStatus ksbonjson_addString(KSBONJSONEncodeContext* ctx,
                                           const char* value,
                                           size_t valueLength)
{
    KSBONJSONContainerState* container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_CHUNKING_STRING();
    SHOULD_NOT_BE_NULL(value);

    container->isExpectingName = !container->isExpectingName;
    return encodeString(ctx, value, valueLength);
}

ksbonjson_encodeStatus ksbonjson_chunkString(KSBONJSONEncodeContext* ctx,
                                             const char* chunk,
                                             size_t chunkLength,
                                             bool isLastChunk)
{
    KSBONJSONContainerState* container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_NULL(chunk);

    unlikely_if(!container->isChunkingString)
    {
        PROPAGATE_ERROR(addByte(ctx, TYPE_STRING));
    }
    container->isChunkingString = !isLastChunk;
    unlikely_if(isLastChunk) // If we're chunking, this is less likely
    {
        container->isExpectingName = !container->isExpectingName;
    }

    return encodeStringChunk(ctx, chunk, chunkLength, isLastChunk);
}

ksbonjson_encodeStatus ksbonjson_addBONJSONDocument(KSBONJSONEncodeContext* ctx,
                                                    const uint8_t* bonjsonDocument,
                                                    size_t documentLength)
{
    KSBONJSONContainerState* container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    container->isExpectingName = true;
    return addBytes(ctx, bonjsonDocument, documentLength);
}

ksbonjson_encodeStatus ksbonjson_beginObject(KSBONJSONEncodeContext* ctx)
{
    KSBONJSONContainerState* container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    PROPAGATE_ERROR(addByte(ctx, TYPE_OBJECT));
    ctx->containerDepth++;
    ctx->containers[ctx->containerDepth] = (KSBONJSONContainerState)
    {
        .isObject = true,
        .isExpectingName = true,
    };
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_beginArray(KSBONJSONEncodeContext* ctx)
{
    KSBONJSONContainerState* container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    PROPAGATE_ERROR(addByte(ctx, TYPE_ARRAY));
    ctx->containerDepth++;
    ctx->containers[ctx->containerDepth] = (KSBONJSONContainerState)
    {
        .isObject = false,
    };
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_endContainer(KSBONJSONEncodeContext* ctx)
{
    KSBONJSONContainerState* container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_VALUE();
    SHOULD_NOT_BE_CHUNKING_STRING();
    unlikely_if(ctx->containerDepth <= 0)
    {
        return KSBONJSON_ENCODE_CLOSED_TOO_MANY_CONTAINERS;
    }

    PROPAGATE_ERROR(addByte(ctx, TYPE_END));
    ctx->containerDepth--;
    container = &ctx->containers[ctx->containerDepth];
    container->isExpectingName = true; // Update parent container.
    return KSBONJSON_ENCODE_OK;
}

const char* ksbonjson_encodeStatusDescription(ksbonjson_encodeStatus status)
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
        case KSBONJSON_ENCODE_COULD_NOT_ADD_DATA:
            return "addBytes() failed to process the passed in data";
        default:
            return "(unknown status - was it a user-defined status code?)";
    }
}
