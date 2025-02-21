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
#include <string.h> // For memcpy()

#pragma GCC diagnostic ignored "-Wpadded"


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
#      error Could not auto-detect machine endianness. Please define KSBONJSON_IS_LITTLE_ENDIAN (0 or 1), or _LITTLE_ENDIAN or _BIG_ENDIAN.
#    endif // __BYTE_ORDER__
// Detect with GLIBC's endian.h
#  elif defined(__GLIBC__)
#    include <endian.h>
#    if (__BYTE_ORDER == __LITTLE_ENDIAN)
#      define KSBONJSON_IS_LITTLE_ENDIAN 1
#    elif (__BYTE_ORDER == __BIG_ENDIAN)
#      define KSBONJSON_IS_LITTLE_ENDIAN 0
#    else
#      error Could not auto-detect machine endianness. Please define KSBONJSON_IS_LITTLE_ENDIAN (0 or 1), or _LITTLE_ENDIAN or _BIG_ENDIAN.
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
#    error Could not auto-detect machine endianness. Please define KSBONJSON_IS_LITTLE_ENDIAN (0 or 1), or _LITTLE_ENDIAN or _BIG_ENDIAN.
#  endif
#endif // KSBONJSON_IS_LITTLE_ENDIAN

// Compiler hints for "if" statements
// #define likely_if(x) if(__builtin_expect(x,1))
#define unlikely_if(x) if(__builtin_expect(x,0))


// ============================================================================
// Types (synced with encoder)
// ============================================================================

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
    SMALLINT_NEGATIVE_EDGE = -106,
    SMALLINT_POSITIVE_EDGE = 106,
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
// Types (internal)
// ============================================================================

typedef struct
{
    uint8_t isObject: 1;
    uint8_t isExpectingName: 1;
    uint8_t isChunkingString: 1;
    uint8_t unused: 5;
} ContainerState;

typedef struct
{
    const uint8_t* const bufferStart;
    const uint8_t* bufferCurrent;
    const uint8_t* const bufferEnd;
    const KSBONJSONDecodeCallbacks* const callbacks;
    void* const userData;
    int containerDepth;
    ContainerState containers[KSBONJSON_MAX_CONTAINER_DEPTH];
} DecodeContext;


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

/**
 * Decode up to 63 bits of ULEB128 data.
 * Returns an error if the data is larger than this.
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
#if KSBONJSON_IS_LITTLE_ENDIAN
    memcpy(bits.b, buf, byteCount);
#else
    for(int i = (int)byteCount-1; i >= 0; i--)
    {
        bits.u64 = (bits.u64 << 8) | buf[i];
    }
#endif
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
    return reportFloat(ctx, decodeFloat16(ctx));
}

static ksbonjson_decodeStatus decodeAndReportFloat32(DecodeContext* const ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(4);
    return reportFloat(ctx, decodeFloat32(ctx));
}

static ksbonjson_decodeStatus decodeAndReportFloat64(DecodeContext* const ctx)
{
    SHOULD_HAVE_ROOM_FOR_BYTES(8);
    return reportFloat(ctx, decodeFloat64(ctx));
}

static ksbonjson_decodeStatus decodeAndReportBigNumber(DecodeContext* const ctx)
{
    uint64_t header = 0;
    PROPAGATE_ERROR(ctx, decodeUleb128(ctx, &header));
    const int sign = fillWithBit0((uint8_t)header);
    const size_t exponentLength = (header >> 1) & 3;
    const size_t significandLength = header >> 3;

    unlikely_if(significandLength > 8)
    {
        return KSBONJSON_DECODE_TOO_BIG;
    }
    unlikely_if(significandLength == 0)
    {
        unlikely_if(exponentLength != 0)
        {
            // Inf or NaN
            return KSBONJSON_DECODE_INVALID_DATA;
        }
        return ctx->callbacks->onBigNumber(ksbonjson_newBigNumber(sign, 0, 0), ctx->userData);
    }

    SHOULD_HAVE_ROOM_FOR_BYTES(significandLength + exponentLength);
    const uint64_t significand = decodeUnsignedInt(ctx, significandLength);
    const int32_t exponent = (int32_t)decodeSignedInt(ctx, exponentLength);

    return ctx->callbacks->onBigNumber(ksbonjson_newBigNumber(sign, significand, exponent), ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportShortString(DecodeContext* const ctx, const uint8_t typeCode)
{
    const uint8_t* const begin = ctx->bufferCurrent;
    const size_t length = (size_t)(typeCode - TYPE_STRING0);

    unlikely_if(begin + length > ctx->bufferEnd)
    {
        return KSBONJSON_DECODE_INCOMPLETE;
    }

    ctx->bufferCurrent += length;
    return ctx->callbacks->onString((const char*)begin, length, ctx->userData);
}

static ksbonjson_decodeStatus decodeAndReportLongString(DecodeContext* const ctx)
{
    const uint8_t* const begin = ctx->bufferCurrent;
    const uint8_t* const end = ctx->bufferEnd;
    const uint8_t* pos = begin;

    for(; pos < end; pos++)
    {
        unlikely_if(*pos == STRING_TERMINATOR)
        {
            const size_t length = (size_t)(pos - ctx->bufferCurrent);
            ctx->bufferCurrent += length + 1;
            return ctx->callbacks->onString((const char*)begin, length, ctx->userData);
        }
    }

    return KSBONJSON_DECODE_INCOMPLETE;
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
        default:
            return ctx->callbacks->onSignedInteger((int8_t)typeCode, ctx->userData);
    }
}

static ksbonjson_decodeStatus decodeDocument(DecodeContext* const ctx)
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

    const ksbonjson_decodeStatus result = decodeDocument(&ctx);
    *decodedOffset = (size_t)(ctx.bufferCurrent - ctx.bufferStart);
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
        case KSBONJSON_DECODE_INVALID_DATA:
            return "Encountered invalid data";
        default:
            return "(unknown status - was it a user-defined status code?)";
    }
}
