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

#pragma GCC diagnostic ignored "-Wfloat-equal"


// ============================================================================
// Helpers
// ============================================================================

/**
 * Best-effort attempt to get the endianness of the machine being compiled for.
 * If this fails, you will have to define it manually.
 *
 * Shamelessly stolen from https://github.com/Tencent/rapidjson/blob/master/include/rapidjson/rapidjson.h
 */
#ifndef KSBONJSON_IS_LITTLE_ENDIAN
// Detect with GCC 4.6's macro
#  ifdef __BYTE_ORDER__
#    if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#      define KSBONJSON_IS_LITTLE_ENDIAN 1
#    elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#      define KSBONJSON_IS_LITTLE_ENDIAN 0
#    else
#      error Unknown machine endianness detected. User needs to define KSBONJSON_IS_LITTLE_ENDIAN.
#    endif // __BYTE_ORDER__
// Detect with GLIBC's endian.h
#  elif defined(__GLIBC__)
#    include <endian.h>
#    if (__BYTE_ORDER == __LITTLE_ENDIAN)
#      define KSBONJSON_IS_LITTLE_ENDIAN 1
#    elif (__BYTE_ORDER == __BIG_ENDIAN)
#      define KSBONJSON_IS_LITTLE_ENDIAN 0
#    else
#      error Unknown machine endianness detected. User needs to define KSBONJSON_IS_LITTLE_ENDIAN.
#   endif // __GLIBC__
// Detect with _LITTLE_ENDIAN and _BIG_ENDIAN macro
#  elif defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
#    define KSBONJSON_IS_LITTLE_ENDIAN 1
#  elif defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)
#    define KSBONJSON_IS_LITTLE_ENDIAN 0
// Detect with architecture macros
#  elif defined(__sparc) || defined(__sparc__) || defined(_POWER) || defined(__powerpc__) || defined(__ppc__) || defined(__ppc64__) || defined(__hpux) || defined(__hppa) || defined(_MIPSEB) || defined(_POWER) || defined(__s390__)
#    define KSBONJSON_IS_LITTLE_ENDIAN 0
#  elif defined(__i386__) || defined(__alpha__) || defined(__ia64) || defined(__ia64__) || defined(_M_IX86) || defined(_M_IA64) || defined(_M_ALPHA) || defined(__amd64) || defined(__amd64__) || defined(_M_AMD64) || defined(__x86_64) || defined(__x86_64__) || defined(_M_X64) || defined(__bfin__)
#    define KSBONJSON_IS_LITTLE_ENDIAN 1
#  elif defined(_MSC_VER) && (defined(_M_ARM) || defined(_M_ARM64))
#    define KSBONJSON_IS_LITTLE_ENDIAN 1
#  else
#    error Unknown machine endianness detected. User needs to define KSBONJSON_IS_LITTLE_ENDIAN.
#  endif
#endif // KSBONJSON_IS_LITTLE_ENDIAN

// Compiler hints for "if" statements
#define likely_if(x) if(__builtin_expect(x,1))
#define unlikely_if(x) if(__builtin_expect(x,0))

enum
{
    TYPE_FLOAT16    = 0x6b,
    TYPE_FLOAT32    = 0x6c,
    TYPE_FLOAT64    = 0x6d,
    TYPE_FALSE      = 0x6e,
    TYPE_TRUE       = 0x6f,
    TYPE_UINT8      = 0x70,
    TYPE_UINT16     = 0x71,
    TYPE_UINT24     = 0x72,
    TYPE_UINT32     = 0x73,
    TYPE_UINT40     = 0x74,
    TYPE_UINT48     = 0x75,
    TYPE_UINT56     = 0x76,
    TYPE_UINT64     = 0x77,
    TYPE_SINT8      = 0x78,
    TYPE_SINT16     = 0x79,
    TYPE_SINT24     = 0x7a,
    TYPE_SINT32     = 0x7b,
    TYPE_SINT40     = 0x7c,
    TYPE_SINT48     = 0x7d,
    TYPE_SINT56     = 0x7e,
    TYPE_SINT64     = 0x7f,
    TYPE_STRING0    = 0x80,
    TYPE_STRING1    = 0x81,
    TYPE_STRING2    = 0x82,
    TYPE_STRING3    = 0x83,
    TYPE_STRING4    = 0x84,
    TYPE_STRING5    = 0x85,
    TYPE_STRING6    = 0x86,
    TYPE_STRING7    = 0x87,
    TYPE_STRING8    = 0x88,
    TYPE_STRING9    = 0x89,
    TYPE_STRING10   = 0x8a,
    TYPE_STRING11   = 0x8b,
    TYPE_STRING12   = 0x8c,
    TYPE_STRING13   = 0x8d,
    TYPE_STRING14   = 0x8e,
    TYPE_STRING15   = 0x8f,
    TYPE_STRING     = 0x90,
    TYPE_BIG_NUMBER = 0x91,
    TYPE_ARRAY      = 0x92,
    TYPE_OBJECT     = 0x93,
    TYPE_END        = 0x94,
    TYPE_NULL       = 0x95,
};

enum
{
    STRING_TERMINATOR = 0xff,
    INTSMALL_NEGATIVE_EDGE = -106,
    INTSMALL_POSITIVE_EDGE = 106,
};

union number_bits
{
    uint8_t  b[8];
    uint32_t u32;
    uint64_t u64;
    int64_t  i64;
    float    f32;
    double   f64;
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

static ksbonjson_encodeStatus encodeNumberType(KSBONJSONEncodeContext* const ctx, const uint8_t typeCode, const uint64_t value, size_t byteCount)
{
    // Allocate 2 unions to give scratch space in front of the encoded value
    union number_bits bits[2];
    // The last byte of our scratch space will hold the type code
    bits[0].b[7] = typeCode;

#if KSBONJSON_IS_LITTLE_ENDIAN
    bits[1].u64 = value;
#else
    for(uint64_t v = value, i = 0; i < byteCount; v >>= 8, i++)
    {
        bits[1].b[i] = (uint8_t)v;
    }
#endif

    return addBytes(ctx, &bits[0].b[7], byteCount + 1);
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

ksbonjson_encodeStatus ksbonjson_addUnsignedInteger(KSBONJSONEncodeContext* const ctx, const uint64_t value)
{
    KSBONJSONContainerState* const container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();
    container->isExpectingName = true;

    if(value <= INTSMALL_POSITIVE_EDGE)
    {
        // Small Int
        return addByte(ctx, (uint8_t)value);
    }

    size_t byteCount = 1;
    for(uint64_t v = value >> 8; v != 0; v >>= 8)
    {
        byteCount++;
    }

    // If the top bit isn't set, save as a signed int
    uint8_t typeCode = ((value >> (byteCount*8-1)) == 0) ? TYPE_SINT8 : TYPE_UINT8;
    return encodeNumberType(ctx, (uint8_t)(typeCode + byteCount - 1), value, byteCount);
}

ksbonjson_encodeStatus ksbonjson_addSignedInteger(KSBONJSONEncodeContext* const ctx, const int64_t value)
{
    if(value >= 0)
    {
        return ksbonjson_addUnsignedInteger(ctx, (uint64_t)value);
    }

    KSBONJSONContainerState* const container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();
    container->isExpectingName = true;

    if(value >= INTSMALL_NEGATIVE_EDGE)
    {
        // Small Int
        return addByte(ctx, (uint8_t)value);
    }

    size_t byteCount = 1;
    for(int64_t v = value >> 8; v != -1; v >>= 8)
    {
        byteCount++;
    }
    if((value << (64 - byteCount * 8)) >= 0)
    {
        // Cutting off this 0xff byte would change the sign
        byteCount++;
    }
    return encodeNumberType(ctx, (uint8_t)(TYPE_SINT8 + byteCount - 1), (uint64_t)value, byteCount);
}

ksbonjson_encodeStatus ksbonjson_addFloat(KSBONJSONEncodeContext* const ctx, const double value)
{
    const int64_t asInt = (int64_t)value;
    unlikely_if((double)asInt == value)
    {
        return ksbonjson_addSignedInteger(ctx, asInt);
    }

    KSBONJSONContainerState* const container = &ctx->containers[ctx->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    union number_bits bits = {.f64 = value};

    // When all exponent bits are set, it signifies an infinite or NaN value
    unlikely_if((bits.u64 & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL)
    {
        // If the significand is 0, it's infinite
        if((bits.u64 & 0x000fffffffffffffULL) == 0)
        {
            return KSBONJSON_ENCODE_INF;
        }
        return KSBONJSON_ENCODE_NAN;
    }

    container->isExpectingName = true;

    bits.f32 = (float)value;
    if((double)bits.f32 == value)
    {
        // Check if we fit into a bfloat16
        bits.u32 &= ~0xffffu;
        if((double)bits.f32 == value)
        {
            bits.u32 >>= 16;
            return encodeNumberType(ctx, TYPE_FLOAT16, bits.u64, 2);
        }
        bits.f32 = (float)value;
        return encodeNumberType(ctx, TYPE_FLOAT32, bits.u64, 4);
    }
    bits.f64 = value;
    return encodeNumberType(ctx, TYPE_FLOAT64, bits.u64, 8);
}

ksbonjson_encodeStatus ksbonjson_addBigNumber(KSBONJSONEncodeContext* const ctx, const KSBigNumber value)
{
    // TODO
    return KSBONJSON_ENCODE_COULD_NOT_ADD_DATA;
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

    if(valueLength <= 15)
    {
        PROPAGATE_ERROR(addByte(ctx, (uint8_t)(TYPE_STRING0 + valueLength)));
        if(valueLength == 0)
        {
            return KSBONJSON_ENCODE_OK;
        }
        return addBytes(ctx, (const uint8_t*)value, valueLength);
    }

    PROPAGATE_ERROR(addByte(ctx, TYPE_STRING));
    PROPAGATE_ERROR(addBytes(ctx, (const uint8_t*)value, valueLength));
    return addByte(ctx, STRING_TERMINATOR);
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
    PROPAGATE_ERROR(addBytes(ctx, (const uint8_t*)chunk, chunkLength));

    likely_if(!isLastChunk)
    {
        container->isChunkingString = true;
        return KSBONJSON_ENCODE_OK;
    }

    container->isChunkingString = false;
    container->isExpectingName = !container->isExpectingName;
    return addByte(ctx, STRING_TERMINATOR);
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
