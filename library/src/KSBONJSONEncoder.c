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

// ABOUTME: BONJSON encoder implementation.
// ABOUTME: Encodes values into the BONJSON binary format with
// ABOUTME: delimiter-terminated containers and CPU-native integer sizes.

#include "KSBONJSONEncoder.h"
#include "KSBONJSONCommon.h"
#include <string.h> // For memcpy()
#ifdef _MSC_VER
#include <intrin.h>
#endif

#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"


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

#define SHOULD_NOT_BE_EXPECTING_OBJECT_NAME(CONTAINER) \
    unlikely_if((CONTAINER)->isObject & (CONTAINER)->isExpectingName) \
        return KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME

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

#if !HAS_BUILTIN(__builtin_clrsbll)
static uint64_t absoluteValue64(int64_t value)
{
    const int64_t mask = value >> (sizeof(value) * 8 - 1);
    return (uint64_t)((value + mask) ^ mask);
}
#endif

/**
 * Count the number of leading zero bits.
 * This will only count up to 63 zero bits because passing 0 to
 * the builtin it calls is UB. You can compensate by adding
 * (!value) to the result if you need max 64.
 */
static size_t leadingZeroBitsMax63(uint64_t value)
{
    value |= 1;

#if HAS_BUILTIN(__builtin_clzll)
    return (size_t)__builtin_clzll(value);
#elif defined(_MSC_VER)
    unsigned long first1 = 0;
    _BitScanReverse64(&first1, value);
    return 63 - first1;
#else
    // Smear set bits right
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;

    // Once we invert, all upper bits are set and all lower bits are clear
    value = ~value;

    // Clear all but the lowest set bit. We now have only one bit set,
    // and log2 of the value is this bit's position.
    value &= -value;

    // Cast to float, then collect the exponent bits (which hold log2 of the value)
    const union num32_bits u = { .f32 = (float)value };
    uint64_t sigBitCount = ((u.u32>>23) - 0x7f) & 0xff;

    // If smearing resulted in all 1 bits, we'd pass 0 into the float and get
    // 0x81 because of how the exponent is encoded, so turn this result into 64.
    sigBitCount = ((sigBitCount&0x7f) ^ (sigBitCount>>7)) | ((sigBitCount&0x80)>>1);

    return 64 - sigBitCount;
#endif
}

static size_t requiredUnsignedIntegerBytesMin1(uint64_t value)
{
    return (63 - leadingZeroBitsMax63(value)) / 8 + 1;
}

/**
 * Round up a byte count to the next CPU-native size (1, 2, 4, or 8).
 */
static size_t roundUpToNativeSize(size_t byteCount)
{
    if(byteCount <= 1) return 1;
    if(byteCount <= 2) return 2;
    if(byteCount <= 4) return 4;
    return 8;
}

/**
 * Map a CPU-native byte count (1, 2, 4, 8) to an index (0, 1, 2, 3).
 */
static size_t nativeSizeToIndex(size_t nativeSize)
{
    // 1->0, 2->1, 4->2, 8->3
    size_t index = 0;
    if(nativeSize >= 2) index = 1;
    if(nativeSize >= 4) index = 2;
    if(nativeSize >= 8) index = 3;
    return index;
}

static size_t requiredSignedIntegerBytesMin1(int64_t value)
{
#if HAS_BUILTIN(__builtin_clrsbll)
    return (63 - (size_t)__builtin_clrsbll(value | 1)) / 8 + 1;
#else
    const size_t leadingZeroBitCount = leadingZeroBitsMax63(absoluteValue64(value));
    const size_t byteCountToRemove = leadingZeroBitCount / 8;
    const int64_t highBytesRemoved = value << byteCountToRemove*8;
    const size_t signDidChange = ((value ^ highBytesRemoved) >> 63) & 1;
    // If the sign changes when cutting out the extra bytes, we need 1 more byte.
    return 8 - byteCountToRemove + signDidChange;
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

static ksbonjson_encodeStatus encodePrimitiveNumeric(KSBONJSONEncodeContext* const ctx,
                                                     const uint8_t typeCode,
                                                     const uint64_t valueBits,
                                                     size_t byteCount)
{
    // Allocate 2 unions to give scratch space in front of the memory-aligned value for the type code
    union num64_bits bits[2];
    // The last byte of our scratch space will hold the type code
    bits[0].b[7] = typeCode;
    bits[1].u64 = toLittleEndian(valueBits);

    return addEncodedBytes(ctx, &bits[0].b[7], byteCount + 1);
}

static ksbonjson_encodeStatus encodeSmallInt(KSBONJSONEncodeContext* const ctx, int64_t value)
{
    // Small int encoding: type_code = value + SMALLINT_BIAS
    return addEncodedByte(ctx, (uint8_t)(value + SMALLINT_BIAS));
}

/**
 * Encode a value using zigzag LEB128 encoding.
 * Zigzag maps signed to unsigned: 0->0, -1->1, 1->2, -2->3, 2->4, etc.
 * LEB128 uses 7 bits per byte with MSB as continuation flag.
 */
static ksbonjson_encodeStatus encodeZigzagLEB128(KSBONJSONEncodeContext* const ctx, int64_t value)
{
    // Zigzag encode: map signed to unsigned
    uint64_t zigzag = (uint64_t)((value << 1) ^ (value >> 63));

    // LEB128 encode
    uint8_t buffer[10]; // Max 10 bytes for 64-bit value
    size_t count = 0;
    do
    {
        uint8_t byte = (uint8_t)(zigzag & 0x7f);
        zigzag >>= 7;
        if(zigzag != 0)
        {
            byte |= 0x80; // Set continuation bit
        }
        buffer[count++] = byte;
    }
    while(zigzag != 0);

    return addEncodedBytes(ctx, buffer, count);
}


// Track state changes after adding a value to an object
static void onValueAdded(KSBONJSONEncodeContext* const ctx)
{
    if(ctx->containerDepth > 0)
    {
        KSBONJSONContainerState* const container = getContainer(ctx);
        if(container->isObject)
        {
            container->isExpectingName = true;
        }
    }
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
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_addBoolean(KSBONJSONEncodeContext* const ctx, const bool value)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME(container);
    container->isExpectingName = container->isObject;

    PROPAGATE_ERROR(addEncodedByte(ctx, value ? TYPE_TRUE : TYPE_FALSE));
    onValueAdded(ctx);
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_addUnsignedInteger(KSBONJSONEncodeContext* const ctx, const uint64_t value)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME(container);
    container->isExpectingName = container->isObject;

    if(value <= SMALLINT_MAX)
    {
        PROPAGATE_ERROR(encodeSmallInt(ctx, (int64_t)value));
        onValueAdded(ctx);
        return KSBONJSON_ENCODE_OK;
    }

    const size_t minBytes = requiredUnsignedIntegerBytesMin1(value);
    const size_t nativeSize = roundUpToNativeSize(minBytes);

    // Use signed type if MSB of the native-sized value is clear (prefer signed over unsigned)
    const uint8_t isMSBSet = (uint8_t)(value >> (nativeSize * 8 - 1));
    if(isMSBSet)
    {
        // Need unsigned type, but if nativeSize would cause MSB to be set,
        // we might need to go to the next larger size for signed.
        // Actually, just use unsigned at this native size.
        const uint8_t typeCode = (uint8_t)(TYPE_UINT8 + nativeSizeToIndex(nativeSize));
        PROPAGATE_ERROR(encodePrimitiveNumeric(ctx, typeCode, value, nativeSize));
    }
    else
    {
        // Can use signed type (positive value, MSB clear)
        const uint8_t typeCode = (uint8_t)(TYPE_SINT8 + nativeSizeToIndex(nativeSize));
        PROPAGATE_ERROR(encodePrimitiveNumeric(ctx, typeCode, value, nativeSize));
    }
    onValueAdded(ctx);
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_addSignedInteger(KSBONJSONEncodeContext* const ctx, const int64_t value)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME(container);
    container->isExpectingName = container->isObject;

    if( (uint64_t)(value-SMALLINT_MIN) <= (uint64_t)(SMALLINT_MAX-SMALLINT_MIN) )
    {
        PROPAGATE_ERROR(encodeSmallInt(ctx, value));
        onValueAdded(ctx);
        return KSBONJSON_ENCODE_OK;
    }

    if(value >= 0)
    {
        // For positive values, compare unsigned vs signed native size
        const size_t unsignedMinBytes = requiredUnsignedIntegerBytesMin1((uint64_t)value);
        const size_t unsignedNativeSize = roundUpToNativeSize(unsignedMinBytes);
        const size_t signedMinBytes = requiredSignedIntegerBytesMin1(value);
        const size_t signedNativeSize = roundUpToNativeSize(signedMinBytes);

        if(unsignedNativeSize < signedNativeSize)
        {
            const uint8_t typeCode = (uint8_t)(TYPE_UINT8 + nativeSizeToIndex(unsignedNativeSize));
            PROPAGATE_ERROR(encodePrimitiveNumeric(ctx, typeCode, (uint64_t)value, unsignedNativeSize));
        }
        else
        {
            // Prefer signed when sizes are equal
            const uint8_t typeCode = (uint8_t)(TYPE_SINT8 + nativeSizeToIndex(signedNativeSize));
            PROPAGATE_ERROR(encodePrimitiveNumeric(ctx, typeCode, (uint64_t)value, signedNativeSize));
        }
    }
    else
    {
        const size_t minBytes = requiredSignedIntegerBytesMin1(value);
        const size_t nativeSize = roundUpToNativeSize(minBytes);
        const uint8_t typeCode = (uint8_t)(TYPE_SINT8 + nativeSizeToIndex(nativeSize));
        PROPAGATE_ERROR(encodePrimitiveNumeric(ctx, typeCode, (uint64_t)value, nativeSize));
    }

    onValueAdded(ctx);
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_addFloat(KSBONJSONEncodeContext* const ctx, const double value)
{
    const int64_t asInt = (int64_t)value;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    if((double)asInt == value)
    {
        // Note: addSignedInteger already handles state tracking
        return ksbonjson_addSignedInteger(ctx, asInt);
    }
#pragma GCC diagnostic pop

    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME(container);

    union num64_bits b64 = {.f64 = value};
    unlikely_if((b64.u64 & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL)
    {
        // When all exponent bits are set, it signifies an infinite or NaN value
        return KSBONJSON_ENCODE_INVALID_DATA;
    }

    container->isExpectingName = container->isObject;

    // Choose optimal float size (no bfloat16 in new spec)
    const union num32_bits b32 = { .f32 = (float)value };
    union num64_bits compare = { .f64 = (double)b32.f32 };
    if(compare.u64 == b64.u64)
    {
        // Fits in float32
        PROPAGATE_ERROR(encodePrimitiveNumeric(ctx, TYPE_FLOAT32, (uint64_t)b32.u32, 4));
    }
    else
    {
        PROPAGATE_ERROR(encodePrimitiveNumeric(ctx, TYPE_FLOAT64, b64.u64, 8));
    }

    onValueAdded(ctx);
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_addBigNumber(KSBONJSONEncodeContext* const ctx, const KSBigNumber value)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME(container);

    container->isExpectingName = container->isObject;

    // Emit type code
    PROPAGATE_ERROR(addEncodedByte(ctx, TYPE_BIG_NUMBER));

    // Emit exponent as zigzag LEB128
    PROPAGATE_ERROR(encodeZigzagLEB128(ctx, (int64_t)value.exponent));

    if(value.significand == 0)
    {
        // Zero significand: signed_length = 0, no magnitude bytes
        PROPAGATE_ERROR(encodeZigzagLEB128(ctx, 0));
    }
    else
    {
        // Compute byte count for magnitude
        const size_t byteCount = requiredUnsignedIntegerBytesMin1(value.significand);
        // signed_length: positive if significand positive, negative if negative
        const int64_t signedLength = (value.significandSign < 0) ? -(int64_t)byteCount : (int64_t)byteCount;
        PROPAGATE_ERROR(encodeZigzagLEB128(ctx, signedLength));

        // Emit magnitude as LE bytes
        union num64_bits bits;
        bits.u64 = toLittleEndian(value.significand);
        PROPAGATE_ERROR(addEncodedBytes(ctx, bits.b, byteCount));
    }

    onValueAdded(ctx);
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_addNull(KSBONJSONEncodeContext* const ctx)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME(container);
    container->isExpectingName = container->isObject;

    PROPAGATE_ERROR(addEncodedByte(ctx, TYPE_NULL));
    onValueAdded(ctx);
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_addString(KSBONJSONEncodeContext* const ctx,
                                           const char* const value,
                                           const size_t valueLength)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_NULL(value);

    // In objects: string can be a key (when expecting name) or value
    const bool isObjectKey = container->isObject && container->isExpectingName;

    // For objects, flip expectation (key -> value -> key)
    if(container->isObject)
    {
        container->isExpectingName = !container->isExpectingName;
    }

    if(valueLength <= 15)
    {
        uint8_t buffer[16];
        buffer[0] = (uint8_t)(TYPE_STRING0 + valueLength);
        memcpy(buffer+1, (const uint8_t*)value, valueLength);
        PROPAGATE_ERROR(addEncodedBytes(ctx, buffer, valueLength+1));
    }
    else
    {
        // Long string: 0xFF + data + 0xFF
        PROPAGATE_ERROR(addEncodedByte(ctx, TYPE_STRINGL));
        PROPAGATE_ERROR(addEncodedBytes(ctx, (const uint8_t*)value, valueLength));
        PROPAGATE_ERROR(addEncodedByte(ctx, TYPE_STRINGL));
    }

    // Only notify value added if this was a value (not a key)
    if(!isObjectKey)
    {
        onValueAdded(ctx);
    }
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_addBONJSONDocument(KSBONJSONEncodeContext* const ctx,
                                                    const uint8_t* const bonjsonDocument,
                                                    const size_t documentLength)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME(container);
    container->isExpectingName = container->isObject;

    PROPAGATE_ERROR(addEncodedBytes(ctx, bonjsonDocument, documentLength));
    onValueAdded(ctx);
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_beginObject(KSBONJSONEncodeContext* const ctx)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME(container);

    unlikely_if(ctx->containerDepth + 1 >= KSBONJSON_MAX_CONTAINER_DEPTH)
    {
        return KSBONJSON_ENCODE_INVALID_DATA;
    }

    ctx->containerDepth++;
    ctx->containers[ctx->containerDepth] = (KSBONJSONContainerState)
    {
        .isObject = true,
        .isExpectingName = true,
    };

    return addEncodedByte(ctx, TYPE_OBJECT);
}

ksbonjson_encodeStatus ksbonjson_beginArray(KSBONJSONEncodeContext* const ctx)
{
    KSBONJSONContainerState* const container = getContainer(ctx);
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME(container);

    unlikely_if(ctx->containerDepth + 1 >= KSBONJSON_MAX_CONTAINER_DEPTH)
    {
        return KSBONJSON_ENCODE_INVALID_DATA;
    }

    ctx->containerDepth++;
    ctx->containers[ctx->containerDepth] = (KSBONJSONContainerState){0};

    return addEncodedByte(ctx, TYPE_ARRAY);
}

ksbonjson_encodeStatus ksbonjson_endContainer(KSBONJSONEncodeContext* const ctx)
{
    unlikely_if(ctx->containerDepth <= 0)
    {
        return KSBONJSON_ENCODE_CLOSED_TOO_MANY_CONTAINERS;
    }

    KSBONJSONContainerState* const container = getContainer(ctx);

    // Cannot close an object while expecting a value for a key
    unlikely_if(container->isObject && !container->isExpectingName)
    {
        return KSBONJSON_ENCODE_EXPECTED_OBJECT_VALUE;
    }

    ctx->containerDepth--;

    PROPAGATE_ERROR(addEncodedByte(ctx, TYPE_END));

    // Notify parent that this container (as a value) was added
    onValueAdded(ctx);

    return KSBONJSON_ENCODE_OK;
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
