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
#include <string.h> // For memcpy()


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

#ifndef HAS_BUILTIN
#   ifdef _MSC_VER
#       define HAS_BUILTIN(A) 0
#   else
#       define HAS_BUILTIN(A) __has_builtin(A)
#   endif
#endif

// Compiler hints for "if" statements
#ifndef likely_if
#   pragma GCC diagnostic ignored "-Wunused-macros"
#   if HAS_BUILTIN(__builtin_expect)
#       define likely_if(x) if(__builtin_expect(x,1))
#       define unlikely_if(x) if(__builtin_expect(x,0))
#   else
#       define likely_if(x) if(x)
#       define unlikely_if(x) if(x)
#   endif
#   pragma GCC diagnostic pop
#endif


// ============================================================================
// Constants (synced with decoder)
// ============================================================================

enum
{
    /* SMALLINT   96 = 0x60 */ TYPE_UINT8  = 0x70,   TYPE_STRING0  = 0x80,   TYPE_RESERVED_90 = 0x90,
    /* SMALLINT   97 = 0x61 */ TYPE_UINT16 = 0x71,   TYPE_STRING1  = 0x81,   TYPE_RESERVED_91 = 0x91,
    /* SMALLINT   98 = 0x62 */ TYPE_UINT24 = 0x72,   TYPE_STRING2  = 0x82,   TYPE_RESERVED_92 = 0x92,
    /* SMALLINT   99 = 0x63 */ TYPE_UINT32 = 0x73,   TYPE_STRING3  = 0x83,   TYPE_RESERVED_93 = 0x93,
    /* SMALLINT  100 = 0x64 */ TYPE_UINT40 = 0x74,   TYPE_STRING4  = 0x84,   TYPE_RESERVED_94 = 0x94,
    TYPE_RESERVED_65 = 0x65,   TYPE_UINT48 = 0x75,   TYPE_STRING5  = 0x85,   TYPE_RESERVED_95 = 0x95,
    TYPE_RESERVED_66 = 0x66,   TYPE_UINT56 = 0x76,   TYPE_STRING6  = 0x86,   TYPE_RESERVED_96 = 0x96,
    TYPE_RESERVED_67 = 0x67,   TYPE_UINT64 = 0x77,   TYPE_STRING7  = 0x87,   TYPE_RESERVED_97 = 0x97,
    TYPE_STRING      = 0x68,   TYPE_SINT8  = 0x78,   TYPE_STRING8  = 0x88,   TYPE_RESERVED_98 = 0x98,
    TYPE_BIG_NUMBER  = 0x69,   TYPE_SINT16 = 0x79,   TYPE_STRING9  = 0x89,   TYPE_ARRAY       = 0x99,
    TYPE_FLOAT16     = 0x6a,   TYPE_SINT24 = 0x7a,   TYPE_STRING10 = 0x8a,   TYPE_OBJECT      = 0x9a,
    TYPE_FLOAT32     = 0x6b,   TYPE_SINT32 = 0x7b,   TYPE_STRING11 = 0x8b,   TYPE_END         = 0x9b,
    TYPE_FLOAT64     = 0x6c,   TYPE_SINT40 = 0x7c,   TYPE_STRING12 = 0x8c,   /* SMALLINT -100 = 0x9c */
    TYPE_NULL        = 0x6d,   TYPE_SINT48 = 0x7d,   TYPE_STRING13 = 0x8d,   /* SMALLINT  -99 = 0x9d */
    TYPE_FALSE       = 0x6e,   TYPE_SINT56 = 0x7e,   TYPE_STRING14 = 0x8e,   /* SMALLINT  -98 = 0x9e */
    TYPE_TRUE        = 0x6f,   TYPE_SINT64 = 0x7f,   TYPE_STRING15 = 0x8f,   /* SMALLINT  -97 = 0x9f */
};

enum
{
    STRING_TERMINATOR = 0xff,
    SMALLINT_NEGATIVE_EDGE = -100,
    SMALLINT_POSITIVE_EDGE = 100,
};


// ============================================================================
// Types
// ============================================================================

union num32_bits
{
    float f32;
    uint32_t u32;
};

union num64_bits
{
    uint8_t  b[8];
    uint64_t u64;
    double   f64;
};


// ============================================================================
// Macros
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

#define SHOULD_NOT_BE_NULL(VALUE) \
    unlikely_if((VALUE) == 0) \
        return KSBONJSON_ENCODE_NULL_POINTER

#define SHOULD_NOT_BE_NULL_OR_CHUNKING_STRING(CONTAINER, VALUE) \
    unlikely_if(((!(VALUE)) | (CONTAINER)->isChunkingString)) \
        return (CONTAINER)->isChunkingString ? KSBONJSON_ENCODE_CHUNKING_STRING : KSBONJSON_ENCODE_NULL_POINTER

#define SHOULD_NOT_BE_EXPECTING_OBJECT_NAME_OR_CHUNKING_STRING(CONTAINER) \
    unlikely_if((((CONTAINER)->isObject & (CONTAINER)->isExpectingName) | (CONTAINER)->isChunkingString)) \
        return (CONTAINER)->isChunkingString ? KSBONJSON_ENCODE_CHUNKING_STRING : KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME

#define SHOULD_NOT_BE_EXPECTING_OBJECT_VALUE_OR_CHUNKING_STRING(CONTAINER) \
    unlikely_if((((CONTAINER)->isObject & !(CONTAINER)->isExpectingName) | (CONTAINER)->isChunkingString)) \
        return (CONTAINER)->isChunkingString ? KSBONJSON_ENCODE_CHUNKING_STRING : KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME

// ============================================================================
// Utility
// ============================================================================

static uint64_t toLittleEndian(uint64_t v)
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

static KSBONJSONContainerState* getContainer(KSBONJSONEncodeContext* const ctx)
{
    return &ctx->containers[ctx->containerDepth];
}

static ksbonjson_encodeStatus addEncodedBytes(KSBONJSONEncodeContext* const ctx,
                                       const uint8_t* const data,
                                       const size_t length)
{
    return ctx->addEncodedData(data, length, ctx->userData);
}

static ksbonjson_encodeStatus addEncodedByte(KSBONJSONEncodeContext* const ctx, const uint8_t value)
{
    return addEncodedBytes(ctx, &value, 1);
}

static ksbonjson_encodeStatus beginContainer(KSBONJSONEncodeContext* const ctx,
                                             const uint8_t typeCode,
                                             const KSBONJSONContainerState containerState)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME_OR_CHUNKING_STRING(container);
    container->isExpectingName = true;

    ctx->containerDepth++;
    ctx->containers[ctx->containerDepth] = containerState;

    return addEncodedByte(ctx, typeCode);
}

static void encodeIntegerIntoBytes(uint64_t value, uint8_t* bytes, size_t byteCount)
{
    value = toLittleEndian(value);
    memcpy(bytes, &value, byteCount);
}

static ksbonjson_encodeStatus encodePrimitiveNumeric(KSBONJSONEncodeContext* const ctx,
                                                     const uint8_t typeCode,
                                                     const uint64_t valueBits,
                                                     size_t byteCount)
{
    // Allocate 2 unions to give scratch space in front of the value for the type code
    union num64_bits bits[2];
    // The last byte of our scratch space will hold the type code
    bits[0].b[7] = typeCode;
    bits[1].u64 = toLittleEndian(valueBits);

    return addEncodedBytes(ctx, &bits[0].b[7], byteCount + 1);
}

static ksbonjson_encodeStatus encodeSmallInt(KSBONJSONEncodeContext* const ctx, int64_t value)
{
    return addEncodedByte(ctx, (uint8_t)value);
}

static uint64_t minBitsToEncodePositiveValue(uint64_t value)
{
	// OR with 1 because a value of 0 still requires 1 bit to encode
	value |= 1;

#if HAS_BUILTIN(__builtin_clzll)
	return (size_t)(64 - __builtin_clzll(value));
#else
	// Smear set bits right
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;

    // Invert to set all higher bits and clear all lower bits
    value = ~value;

	// Clear all but the lowest set bit
    value &= -value;

    // Cast to float, then collect the exponent bits (which hold log2 of the value)
    union num32_bits u = { .f32 = (float)value };
    uint64_t sigBitCount = ((u.u32>>23) - 0x7f) & 0xff;

    // If smearing resulted in all 1 bits, we'd pass 0 into the float and get
    // 0x81 because of how the exponent is encoded, so turn this result into 64.
	return ((sigBitCount&0x7f) ^ (sigBitCount>>7)) | ((sigBitCount&0x80)>>1);
#endif
}

static size_t minBytesToEncodePositiveValue(uint64_t value)
{
    return (minBitsToEncodePositiveValue(value)-1) / 8 + 1;
}

static size_t minBytesToEncodeNegativeValue(int64_t value)
{
#if HAS_BUILTIN(__builtin_clrsbll)
    return 8 - (unsigned)__builtin_clrsbll(value) / 8;
#else
    size_t byteCount = minBytesToEncodePositiveValue((uint64_t)-value);
    unlikely_if((value << (64 - byteCount*8)) > 0)
    {
        // If cutting off the upper bytes inverts the sign, we need to encode another byte.
        byteCount++;
    }

	return byteCount;
#endif
}

static uint64_t absoluteValue64(int64_t value)
{
    const int64_t mask = value >> (sizeof(value) * 8 - 1);
    return (uint64_t)((value + mask) ^ mask);
}


// ============================================================================
// API
// ============================================================================

void ksbonjson_beginEncode(KSBONJSONEncodeContext* const ctx,
                           const KSBONJSONAddEncodedDataFunc addEncodedBytesFunc,
                           void* const userData)
{
    *ctx = (KSBONJSONEncodeContext){0};
    ctx->addEncodedData = addEncodedBytesFunc;
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
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME_OR_CHUNKING_STRING(container);
    container->isExpectingName = true;

    return addEncodedByte(ctx, value ? TYPE_TRUE : TYPE_FALSE);
}

ksbonjson_encodeStatus ksbonjson_addUnsignedInteger(KSBONJSONEncodeContext* const ctx, const uint64_t value)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME_OR_CHUNKING_STRING(container);
    container->isExpectingName = true;

    if(value <= SMALLINT_POSITIVE_EDGE)
    {
        return encodeSmallInt(ctx, (int64_t)value);
    }

    const size_t byteCount = minBytesToEncodePositiveValue(value);

    // If the MSB is cleared, save as a signed int (prefer signed over unsigned)
    uint8_t typeCode = (uint8_t)(TYPE_SINT8 - (value >> (byteCount*8-1)) * 8);

    return encodePrimitiveNumeric(ctx, (uint8_t)(typeCode + byteCount - 1), value, byteCount);
}

ksbonjson_encodeStatus ksbonjson_addSignedInteger(KSBONJSONEncodeContext* const ctx, const int64_t value)
{
    if(value >= 0)
    {
        return ksbonjson_addUnsignedInteger(ctx, (uint64_t)value);
    }

    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME_OR_CHUNKING_STRING(container);
    container->isExpectingName = true;

    if(value >= SMALLINT_NEGATIVE_EDGE)
    {
        return encodeSmallInt(ctx, value);
    }

    const size_t byteCount = minBytesToEncodeNegativeValue(value);

    return encodePrimitiveNumeric(ctx, (uint8_t)(TYPE_SINT8 + byteCount - 1), (uint64_t)value, byteCount);
}

ksbonjson_encodeStatus ksbonjson_addFloat(KSBONJSONEncodeContext* const ctx, const double value)
{
    const int64_t asInt = (int64_t)value;

#pragma GCC diagnostic ignored "-Wfloat-equal"
    unlikely_if((double)asInt == value)
    {
        return ksbonjson_addSignedInteger(ctx, asInt);
    }
#pragma GCC diagnostic pop

    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME_OR_CHUNKING_STRING(container);

    union num64_bits b64 = {.f64 = value};
    unlikely_if((b64.u64 & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL)
    {
        // When all exponent bits are set, it signifies an infinite or NaN value
        return KSBONJSON_ENCODE_INVALID_DATA;
    }

    container->isExpectingName = true;

    // Choose optimal float size
    const union num32_bits b32 = { .f32 = (float)value };
    const union num32_bits b16 = { .u32 = b32.u32 & ~0xffffu };
    union num64_bits compare = { .f64 = (double)b16.f32 };
    const unsigned isF16 = !(compare.u64 ^ b64.u64);
    compare.f64 = (double)b32.f32;
    const unsigned isF32 = !(compare.u64 ^ b64.u64) & !isF16;
    const unsigned isF64 = !isF32 & !isF16;

    // Calculate arguments
    const uint8_t typeCode = (uint8_t)(TYPE_FLOAT16 + isF32 + isF64*2);
    const unsigned byteCount = 2 + 2*isF32 + 6*isF64;

    // Choose which bits to encode
    const uint64_t mask = (isF16*0xffff) | (isF32*0xffffffff);
    const uint32_t as16Or32Bit = b32.u32 >> (16*isF16);
    b64.u64 = (b64.u64&(~mask)) | (as16Or32Bit & mask);

    return encodePrimitiveNumeric(ctx, typeCode, b64.u64, byteCount);
}

ksbonjson_encodeStatus ksbonjson_addBigNumber(KSBONJSONEncodeContext* const ctx, const KSBigNumber value)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME_OR_CHUNKING_STRING(container);

    unlikely_if(value.exponent < -0x800000 || value.exponent > 0x7fffff)
    {
        return KSBONJSON_ENCODE_INVALID_DATA;
    }

    const size_t exponentByteCount = value.exponent ? minBytesToEncodePositiveValue(absoluteValue64(value.exponent)) : 0;
    const size_t significandByteCount = value.significand ? minBytesToEncodePositiveValue(value.significand) : 0;

    //   Header Byte
    // ───────────────
    // S S S S S E E N
    // ╰─┴─┼─┴─╯ ╰─┤ ╰─> Significand sign (0 = positive, 1 = negative)
    //     │       ╰───> Exponent Length (0-3 bytes)
    //     ╰───────────> Significand Length (0-31 bytes, but will never exceed 8)

    // Allocate 2 unions to give scratch space in front of the significand for the exponent, header, and type code
    union num64_bits bits[2];
    bits[1].u64 = toLittleEndian(value.significand);
    encodeIntegerIntoBytes((uint64_t)value.exponent, bits[0].b+8-exponentByteCount, exponentByteCount);
    bits[0].b[8-exponentByteCount-1] = (uint8_t)
        (
            ((value.significandSign >> 31) & 1) |
            (exponentByteCount << 1) |
            (significandByteCount << 3)
        );
    bits[0].b[8-exponentByteCount-2] = TYPE_BIG_NUMBER;

    return addEncodedBytes(ctx, bits[0].b+8-exponentByteCount-2, significandByteCount + exponentByteCount + 2);
}

ksbonjson_encodeStatus ksbonjson_addNull(KSBONJSONEncodeContext* const ctx)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME_OR_CHUNKING_STRING(container);
    container->isExpectingName = true;

    return addEncodedByte(ctx, TYPE_NULL);
}

ksbonjson_encodeStatus ksbonjson_addString(KSBONJSONEncodeContext* const ctx,
                                           const char* const value,
                                           const size_t valueLength)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_NULL_OR_CHUNKING_STRING(container, value);

    // String can be a name or value, so flip expectation
    container->isExpectingName = !container->isExpectingName;

    if(valueLength <= 15)
    {
        uint8_t buffer[16];
        buffer[0] = (uint8_t)(TYPE_STRING0 + valueLength);
        memcpy(buffer+1, (const uint8_t*)value, valueLength);
        return addEncodedBytes(ctx, buffer, valueLength+1);
    }

    PROPAGATE_ERROR(addEncodedByte(ctx, TYPE_STRING));
    PROPAGATE_ERROR(addEncodedBytes(ctx, (const uint8_t*)value, valueLength));
    return addEncodedByte(ctx, STRING_TERMINATOR);
}

ksbonjson_encodeStatus ksbonjson_chunkString(KSBONJSONEncodeContext* const ctx,
                                             const char* const chunk,
                                             const size_t chunkLength,
                                             const bool isLastChunk)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_NULL(chunk);

    unlikely_if(!container->isChunkingString)
    {
        // We weren't already chunking, so start a new chunked string
        PROPAGATE_ERROR(addEncodedByte(ctx, TYPE_STRING));
    }

    PROPAGATE_ERROR(addEncodedBytes(ctx, (const uint8_t*)chunk, chunkLength));

    likely_if(!isLastChunk)
    {
        container->isChunkingString = true;
        return KSBONJSON_ENCODE_OK;
    }

    // This is the last string chunk, so terminate the string
    container->isChunkingString = false;

    // String can be a name or value, so flip expectation
    container->isExpectingName = !container->isExpectingName;

    return addEncodedByte(ctx, STRING_TERMINATOR);
}

ksbonjson_encodeStatus ksbonjson_addBONJSONDocument(KSBONJSONEncodeContext* const ctx,
                                                    const uint8_t* const bonjsonDocument,
                                                    const size_t documentLength)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME_OR_CHUNKING_STRING(container);
    container->isExpectingName = true;

    return addEncodedBytes(ctx, bonjsonDocument, documentLength);
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
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_VALUE_OR_CHUNKING_STRING(container);

    unlikely_if(ctx->containerDepth <= 0)
    {
        return KSBONJSON_ENCODE_CLOSED_TOO_MANY_CONTAINERS;
    }

    ctx->containerDepth--;
    return addEncodedByte(ctx, TYPE_END);
}

const char* ksbonjson_describeEncodeStatus(const ksbonjson_encodeStatus status)
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
        case KSBONJSON_ENCODE_INVALID_DATA:
            return "The object to encode contains invalid data";
        case KSBONJSON_ENCODE_COULD_NOT_ADD_DATA:
            return "addEncodedBytes() failed to process the passed in data";
        default:
            return "(unknown status - was it a user-defined status code?)";
    }
}
