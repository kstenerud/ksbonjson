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


// ============================================================================
// Helpers
// ============================================================================

// Compiler hints for "if" statements
#define likely_if(x) if(__builtin_expect(x,1))
#define unlikely_if(x) if(__builtin_expect(x,0))

enum {
    TYPE_NULL = 0x6a,
    TYPE_INT16 = 0x6b,
    TYPE_FLOAT32 = 0x6c,
    TYPE_FLOAT64 = 0x6d,
    TYPE_BIGPOSITIVE = 0x6e,
    TYPE_BIGNEGATIVE = 0x6f,
    TYPE_STRINGSHORT = 0x70,
    TYPE_STRINGLONG = 0x90,
    TYPE_ARRAY = 0x91,
    TYPE_OBJECT = 0x92,
    TYPE_END = 0x93,
    TYPE_FALSE = 0x94,
    TYPE_TRUE = 0x95,
};

#define SMALLINT_MAX 0x69
#define SMALLINT_MIN 0x96

union uint16_u
{
    uint16_t u16;
    uint8_t b[2];
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
    uint8_t isInObject: 1;
    uint8_t isExpectingName: 1;
    uint8_t isChunkingString: 1;
} DecodeFrame;

typedef struct
{
    int stackDepth;
    int maxDepth;
    DecodeFrame stack[KSBONJSON_MAX_CONTAINER_DEPTH];
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

static ksbonjson_decodeStatus decodeAndReportInt16(DecodeContext* ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(2);
    int16_t value = (int16_t)((uint16_t)ctx->bufferCurrent[0] |
                             ((uint16_t)ctx->bufferCurrent[1]<<8)
                             );
    ctx->bufferCurrent += 2;
    return ctx->callbacks->onInteger(value, ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportFloat32(DecodeContext* ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(4);
    union float32_u u = {
        .u32 = (uint32_t)((uint32_t)ctx->bufferCurrent[0] |
                          ((uint32_t)ctx->bufferCurrent[1]<<8) |
                          ((uint32_t)ctx->bufferCurrent[2]<<16) |
                          ((uint32_t)ctx->bufferCurrent[3]<<24)
                          )
    };
    ctx->bufferCurrent += 4;
    return ctx->callbacks->onFloat(u.f32, ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportFloat64(DecodeContext* ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(8);
    union float64_u u = {
        .u64 = (uint64_t)((uint64_t)ctx->bufferCurrent[0] |
                          ((uint64_t)ctx->bufferCurrent[1]<<8) |
                          ((uint64_t)ctx->bufferCurrent[2]<<16) |
                          ((uint64_t)ctx->bufferCurrent[3]<<24) |
                          ((uint64_t)ctx->bufferCurrent[4]<<32) |
                          ((uint64_t)ctx->bufferCurrent[5]<<40) |
                          ((uint64_t)ctx->bufferCurrent[6]<<48) |
                          ((uint64_t)ctx->bufferCurrent[7]<<56)
                          )
    };
    ctx->bufferCurrent += 8;
    return ctx->callbacks->onFloat(u.f64, ctx->userData);
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
    unlikely_if(significand > INT64_MAX)
    {
        return KSBONJSON_DECODE_TOO_BIG;
    }
    return ctx->callbacks->onInteger(-((int64_t)significand), ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportShortString(DecodeContext* ctx, uint8_t typeCode)
{
    size_t length = typeCode - TYPE_STRINGSHORT;
    SHOULD_HAVE_ROOM_FOR_BYTES(length);
    char* string = (char*)ctx->bufferCurrent;
    ctx->bufferCurrent += length;
    return ctx->callbacks->onString(string, length, ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportLongString(DecodeContext* ctx)
{
    uint64_t header = 0;
    PROPAGATE_ERROR(ctx, decodeUleb128(ctx, &header));
    bool continuation = header & 1;
    uint64_t length = header >> 1;
    SHOULD_HAVE_ROOM_FOR_BYTES(length);
    const char* str = (char*)ctx->bufferCurrent;
    ctx->bufferCurrent += length;
    const KSBONJSONDecodeCallbacks* callbacks = ctx->callbacks;
    if(!continuation)
    {
        return callbacks->onString(str, length, ctx->userData);
    }

    // TODO: chunked element name...
    // TODO: Max name length

    PROPAGATE_ERROR(ctx, callbacks->onStringChunk(str,
                                                  length,
                                                  !continuation,
                                                  ctx->userData));
    while(continuation)
    {
        PROPAGATE_ERROR(ctx, decodeUleb128(ctx, &header));
        continuation = header & 1;
        length = header >> 1;
        SHOULD_HAVE_ROOM_FOR_BYTES(length);
        PROPAGATE_ERROR(ctx, callbacks->onStringChunk((char*)ctx->bufferCurrent,
                                                      length,
                                                      !continuation,
                                                      ctx->userData));
    }

    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus stackContainer(DecodeContext* ctx, bool isObject)
{
    if(ctx->stackDepth > KSBONJSON_MAX_CONTAINER_DEPTH)
    {
        return KSBONJSON_DECODE_CONTAINER_DEPTH_EXCEEDED;
    }
    ctx->stackDepth++;
    DecodeFrame* frame = &ctx->stack[ctx->stackDepth];
    frame->isChunkingString = false;
    frame->isExpectingName = isObject;
    frame->isInObject = isObject;

    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus unstackContainer(DecodeContext* ctx)
{
    if(ctx->stackDepth <= 0)
    {
        return KSBONJSON_DECODE_UNBALANCED_CONTAINERS;
    }
    ctx->stackDepth--;

    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus decode(DecodeContext* const ctx)
{
    const KSBONJSONDecodeCallbacks* callbacks = ctx->callbacks;
    void* userData = ctx->userData;

    #define SHOULD_NOT_BE_EXPECTING_NAME() \
        do \
        { \
            if(currentFrame->isInObject && currentFrame->isExpectingName) \
            { \
                return KSBONJSON_DECODE_EXPECTED_STRING; \
            } \
        } \
        while(0)

    while(ctx->bufferCurrent < ctx->bufferEnd)
    {
        DecodeFrame* currentFrame = &ctx->stack[ctx->stackDepth];
        uint8_t typeCode = *ctx->bufferCurrent++;
        if(typeCode <= SMALLINT_MAX)
        {
            SHOULD_NOT_BE_EXPECTING_NAME();
            PROPAGATE_ERROR(ctx, callbacks->onUInteger(typeCode, userData));
        }
        else if(typeCode >= SMALLINT_MIN)
        {
            SHOULD_NOT_BE_EXPECTING_NAME();
            PROPAGATE_ERROR(ctx, callbacks->onInteger((int8_t)typeCode, userData));
        }
        else if(typeCode >= TYPE_STRINGSHORT && typeCode < TYPE_STRINGSHORT + 32)
        {
            PROPAGATE_ERROR(ctx, decodeAndReportShortString(ctx, typeCode));
        }
        else switch(typeCode)
        {
            case TYPE_STRINGLONG:
                PROPAGATE_ERROR(ctx, decodeAndReportLongString(ctx));
                break;
            case TYPE_NULL:
                SHOULD_NOT_BE_EXPECTING_NAME();
                PROPAGATE_ERROR(ctx, callbacks->onNull(userData));
                break;
            case TYPE_INT16:
                SHOULD_NOT_BE_EXPECTING_NAME();
                PROPAGATE_ERROR(ctx, decodeAndReportInt16(ctx));
                break;
            case TYPE_FLOAT32:
                SHOULD_NOT_BE_EXPECTING_NAME();
                PROPAGATE_ERROR(ctx, decodeAndReportFloat32(ctx));
                break;
            case TYPE_FLOAT64:
                SHOULD_NOT_BE_EXPECTING_NAME();
                PROPAGATE_ERROR(ctx, decodeAndReportFloat64(ctx));
                break;
            case TYPE_BIGPOSITIVE:
                SHOULD_NOT_BE_EXPECTING_NAME();
                PROPAGATE_ERROR(ctx, decodeAndReportPositiveBigNumber(ctx));
                break;
            case TYPE_BIGNEGATIVE:
                SHOULD_NOT_BE_EXPECTING_NAME();
                PROPAGATE_ERROR(ctx, decodeAndReportNegativeBigNumber(ctx));
                break;
            case TYPE_ARRAY:
                SHOULD_NOT_BE_EXPECTING_NAME();
                PROPAGATE_ERROR(ctx, callbacks->onBeginArray(userData));
                PROPAGATE_ERROR(ctx, stackContainer(ctx, false));
                break;
            case TYPE_OBJECT:
                SHOULD_NOT_BE_EXPECTING_NAME();
                PROPAGATE_ERROR(ctx, callbacks->onBeginObject(userData));
                PROPAGATE_ERROR(ctx, stackContainer(ctx, true));
                break;
            case TYPE_END:
                PROPAGATE_ERROR(ctx, callbacks->onEndContainer(userData));
                PROPAGATE_ERROR(ctx, unstackContainer(ctx));
                break;
            case TYPE_FALSE:
                SHOULD_NOT_BE_EXPECTING_NAME();
                PROPAGATE_ERROR(ctx, callbacks->onBoolean(false, userData));
                break;
            case TYPE_TRUE:
                SHOULD_NOT_BE_EXPECTING_NAME();
                PROPAGATE_ERROR(ctx, callbacks->onBoolean(true, userData));
                break;
        }
        currentFrame->isExpectingName = !currentFrame->isExpectingName;
    }

    if(ctx->stackDepth > 0)
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
    DecodeContext context =
    {
        .bufferStart = document,
        .bufferCurrent = document,
        .bufferEnd = document + documentLength,
        .callbacks = callbacks,
        .userData = userData,
    };

    ksbonjson_decodeStatus result = decode(&context);
    *decodedOffset = context.bufferCurrent - context.bufferStart;
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
        case KSBONJSON_DECODE_COULD_NOT_PROCESS_DATA:
            return "A callback failed to process the passed in data";
        default:
            return "(unknown status - was it a user-defined status code?)";
    }
}
