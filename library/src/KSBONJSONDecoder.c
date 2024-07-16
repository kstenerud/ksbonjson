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
    const uint8_t* bufferStart;
    const uint8_t* bufferCurrent;
    const uint8_t* bufferEnd;
    const KSBONJSONDecodeCallbacks* const callbacks;
    void* userData;
} DecodeContext;

#define PROPAGATE_ERROR(CONTEXT, CALL) \
    do \
    { \
        ksbonjson_decodeStatus propagatedResult = CALL; \
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
static ksbonjson_decodeStatus decodeUleb128(DecodeContext* ctx, uint64_t* valuePtr)
{
    uint64_t nextByte = 0;
    uint64_t value = 0;
    int shift = 0;
    do
    {
        SHOULD_HAVE_ROOM_FOR_BYTES(1);
        nextByte = *ctx->bufferCurrent++;
        uint64_t nextSegment = (nextByte&0x7f);
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

static ksbonjson_decodeStatus decodeBigNumber(DecodeContext* ctx, uint64_t* pSignificand, int* pExponent)
{
    uint64_t header = 0;
    PROPAGATE_ERROR(ctx, decodeUleb128(ctx, &header));
    int exponentLength = header & 3;
    uint64_t significandLength = header >> 2;

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

static ksbonjson_decodeStatus decodeAndReportIntSmall(DecodeContext* ctx, int value)
{
    return ctx->callbacks->onInteger(value - INTSMALL_BIAS, ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportInt8(DecodeContext* ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(1);
    int value = (int8_t)*ctx->bufferCurrent;
    ctx->bufferCurrent += 1;

    if(value < 0)
    {
        return ctx->callbacks->onInteger(value - INTSMALL_BIAS, ctx->userData);
    }
    return ctx->callbacks->onInteger(value + (INTSMALL_BIAS+1), ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportInt(DecodeContext* ctx, int size)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(size);
    const uint8_t* buf = ctx->bufferCurrent;
    ctx->bufferCurrent += size;

#if KSBONJSON_IS_LITTLE_ENDIAN
    // Use the highest byte to sign-extend init the int64
    union int64_u u = {.i64 = (int8_t)buf[size-1]};
    memcpy(u.b, buf, size);
#else
    // Use the highest byte to sign-extend init the int64
    union int64_u u = {.i64 = (int8_t)buf[0]};
    for(int i = 0; i < size; i++)
    {
        u.b[i] = buf[(size-1) - i];
    }
#endif
    return ctx->callbacks->onInteger(u.i64, ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportUInt64(DecodeContext* ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(8);
    const uint8_t* buf = ctx->bufferCurrent;
    ctx->bufferCurrent += 8;

#if KSBONJSON_IS_LITTLE_ENDIAN
    uint64_t value;
    memcpy(&value, buf, sizeof(value));
    return ctx->callbacks->onUInteger(value, ctx->userData);
#else
    union uint64_u u = {.b = {buf[7], buf[6], buf[5], buf[4], buf[3], buf[2], buf[1], buf[0]}};
    return ctx->callbacks->onUInteger(u.u64, ctx->userData);
#endif
}

static ksbonjson_decodeStatus decodeAndReportFloat16(DecodeContext* ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(2);
    const uint8_t* buf = ctx->bufferCurrent;
    ctx->bufferCurrent += 2;

#if KSBONJSON_IS_LITTLE_ENDIAN
    union float32_u u = {.b = {0, 0, buf[0], buf[1]}};
    return ctx->callbacks->onFloat(u.f32, ctx->userData);
#else
    union float32_u u = {.b = {buf[1], buf[0], 0, 0}};
    return ctx->callbacks->onFloat(u.f32, ctx->userData);
#endif
}

static ksbonjson_decodeStatus decodeAndReportFloat32(DecodeContext* ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(4);
    const uint8_t* buf = ctx->bufferCurrent;
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

static ksbonjson_decodeStatus decodeAndReportFloat64(DecodeContext* ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(8);
    const uint8_t* buf = ctx->bufferCurrent;
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

static ksbonjson_decodeStatus decodeAndReportPositiveBigNumber(DecodeContext* ctx)
{
    uint64_t significand = 0;
    int exponent = 0;
    PROPAGATE_ERROR(ctx, decodeBigNumber(ctx, &significand, &exponent));
    // TODO: handle exponent
    return ctx->callbacks->onUInteger(significand, ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportNegativeBigNumber(DecodeContext* ctx)
{
    uint64_t significand = 0;
    int exponent = 0;
    PROPAGATE_ERROR(ctx, decodeBigNumber(ctx, &significand, &exponent));
    // TODO: handle exponent
    unlikely_if(significand > 0x8000000000000000ULL)
    {
        return KSBONJSON_DECODE_TOO_BIG;
    }
    return ctx->callbacks->onInteger(-((int64_t)significand), ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportString(DecodeContext* ctx)
{
    const uint8_t* pos = ctx->bufferCurrent;
    const char* begin = (const char*)pos;
    const uint8_t* end = ctx->bufferEnd;

    for(; pos < end; pos++)
    {
        if(*pos == TYPE_STRING)
        {
            size_t length = pos - ctx->bufferCurrent;
            ctx->bufferCurrent += length + 1;
            return ctx->callbacks->onString(begin, length, ctx->userData);
        }
    }

    return KSBONJSON_DECODE_INCOMPLETE;
}

static ksbonjson_decodeStatus beginContainer(DecodeContext* ctx, bool isObject)
{
    unlikely_if(ctx->containerDepth > KSBONJSON_MAX_CONTAINER_DEPTH)
    {
        return KSBONJSON_DECODE_CONTAINER_DEPTH_EXCEEDED;
    }
    ctx->containerDepth++;
    ContainerState* container = &ctx->containers[ctx->containerDepth];
    container->isChunkingString = false;
    container->isExpectingName = isObject;
    container->isObject = isObject;

    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus endContainer(DecodeContext* ctx)
{
    unlikely_if(ctx->containerDepth <= 0)
    {
        return KSBONJSON_DECODE_UNBALANCED_CONTAINERS;
    }
    ctx->containerDepth--;

    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus decode(DecodeContext* const ctx)
{
    const KSBONJSONDecodeCallbacks* callbacks = ctx->callbacks;
    void* userData = ctx->userData;

    #define SHOULD_NOT_BE_EXPECTING_NAME() \
        do \
        { \
            unlikely_if(currentFrame->isObject && currentFrame->isExpectingName) \
            { \
                return KSBONJSON_DECODE_EXPECTED_OBJECT_NAME; \
            } \
        } \
        while(0)

    while(ctx->bufferCurrent < ctx->bufferEnd)
    {
        ContainerState* currentFrame = &ctx->containers[ctx->containerDepth];
        uint8_t typeCode = *ctx->bufferCurrent++;
        if(typeCode <= INTSMALL_MAX)
        {
            SHOULD_NOT_BE_EXPECTING_NAME();
            PROPAGATE_ERROR(ctx, decodeAndReportIntSmall(ctx, typeCode));
        }
        else if(typeCode == TYPE_STRING)
        {
            PROPAGATE_ERROR(ctx, decodeAndReportString(ctx));
        }
        else if(typeCode == TYPE_END)
        {
            unlikely_if(currentFrame->isObject && !currentFrame->isExpectingName)
            {
                return KSBONJSON_DECODE_EXPECTED_OBJECT_VALUE;
            }
            PROPAGATE_ERROR(ctx, callbacks->onEndContainer(userData));
            PROPAGATE_ERROR(ctx, endContainer(ctx));
        }
        else
        {
            SHOULD_NOT_BE_EXPECTING_NAME();
            switch(typeCode)
            {
                case TYPE_STRING:
                    break;
                case TYPE_NULL:
                    PROPAGATE_ERROR(ctx, callbacks->onNull(userData));
                    break;
                case TYPE_INT8:
                    PROPAGATE_ERROR(ctx, decodeAndReportInt8(ctx));
                    break;
                case TYPE_INT16:
                    PROPAGATE_ERROR(ctx, decodeAndReportInt(ctx, 2));
                    break;
                case TYPE_INT24:
                    PROPAGATE_ERROR(ctx, decodeAndReportInt(ctx, 3));
                    break;
                case TYPE_INT32:
                    PROPAGATE_ERROR(ctx, decodeAndReportInt(ctx, 4));
                    break;
                case TYPE_INT40:
                    PROPAGATE_ERROR(ctx, decodeAndReportInt(ctx, 5));
                    break;
                case TYPE_INT48:
                    PROPAGATE_ERROR(ctx, decodeAndReportInt(ctx, 6));
                    break;
                case TYPE_INT56:
                    PROPAGATE_ERROR(ctx, decodeAndReportInt(ctx, 7));
                    break;
                case TYPE_INT64:
                    PROPAGATE_ERROR(ctx, decodeAndReportInt(ctx, 8));
                    break;
                case TYPE_UINT64:
                    PROPAGATE_ERROR(ctx, decodeAndReportUInt64(ctx));
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
                    PROPAGATE_ERROR(ctx, callbacks->onBeginArray(userData));
                    PROPAGATE_ERROR(ctx, beginContainer(ctx, false));
                    break;
                case TYPE_OBJECT:
                    PROPAGATE_ERROR(ctx, callbacks->onBeginObject(userData));
                    PROPAGATE_ERROR(ctx, beginContainer(ctx, true));
                    break;
                case TYPE_FALSE:
                    PROPAGATE_ERROR(ctx, callbacks->onBoolean(false, userData));
                    break;
                case TYPE_TRUE:
                    PROPAGATE_ERROR(ctx, callbacks->onBoolean(true, userData));
                    break;
            }
        }
        currentFrame->isExpectingName = !currentFrame->isExpectingName;
    }

    unlikely_if(ctx->containerDepth > 0)
    {
        return KSBONJSON_DECODE_UNCLOSED_CONTAINERS;
    }
    return callbacks->onEndData(userData);
}


// ============================================================================
// API
// ============================================================================

ksbonjson_decodeStatus ksbonjson_decode(const uint8_t* document,
                                        size_t documentLength,
                                        const KSBONJSONDecodeCallbacks* callbacks,
                                        void* userData,
                                        size_t* decodedOffset)
{
    DecodeContext ctx =
    {
        .bufferStart = document,
        .bufferCurrent = document,
        .bufferEnd = document + documentLength,
        .callbacks = callbacks,
        .userData = userData,
    };

    ksbonjson_decodeStatus result = decode(&ctx);
    *decodedOffset = ctx.bufferCurrent - ctx.bufferStart;
    return result;
}

const char* ksbonjson_decodeStatusDescription(ksbonjson_decodeStatus status)
{
    switch (status)
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
