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

#if KSBONJSON_USE_MEMCPY
#include <string.h>
#endif


// ============================================================================
// Helpers
// ============================================================================

// Compiler hints for "if" statements
#define likely_if(x) if(__builtin_expect(x,1))
#define unlikely_if(x) if(__builtin_expect(x,0))

enum {
    TYPE_NULL = 0x69,
    TYPE_INT16 = 0x6a,
    TYPE_INT32 = 0x6b,
    TYPE_INT64 = 0x6c,
    TYPE_UINT64 = 0x6d,
    TYPE_FLOAT32 = 0x6e,
    TYPE_FLOAT64 = 0x6f,
    TYPE_STRINGSHORT = 0x70,
    TYPE_STRINGLONG = 0x90,
    TYPE_ARRAY = 0x91,
    TYPE_OBJECT = 0x92,
    TYPE_END = 0x93,
    TYPE_FALSE = 0x94,
    TYPE_TRUE = 0x95,
    TYPE_BIGPOSITIVE = 0x96,
    TYPE_BIGNEGATIVE = 0x97,
};

#define SMALLINT_MAX 104
#define SMALLINT_MIN -104

union uint16_u
{
    uint16_t u16;
    uint8_t b[2];
};

union uint32_u
{
    uint32_t u32;
    uint8_t b[4];
};

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
    unlikely_if(!state->isInObject) \
    { \
        return KSBONJSON_ENCODE_NOT_IN_AN_OBJECT; \
    }

#define SHOULD_NOT_BE_EXPECTING_OBJECT_NAME() \
    unlikely_if(state->isInObject \
    && state->isExpectingName) \
    { \
        return KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME; \
    }

#define SHOULD_NOT_BE_EXPECTING_OBJECT_VALUE() \
    unlikely_if(state->isInObject \
    && !state->isExpectingName) \
    { \
        return KSBONJSON_ENCODE_EXPECTED_OBJECT_VALUE; \
    }

#define SHOULD_NOT_BE_CHUNKING_STRING() \
    unlikely_if(state->isChunkingString) \
    { \
        return KSBONJSON_ENCODE_CHUNKING_STRING; \
    }

#define SHOULD_NOT_BE_NULL(VALUE) \
    unlikely_if((VALUE) == NULL) \
    { \
        return KSBONJSON_ENCODE_NULL_POINTER; \
    }

static const union uint16_u endiannessMarker = {.u16 = 0xff00};
static inline bool isLittleEndian() {
    return endiannessMarker.b[0] == 0;
}

static ksbonjson_encodeStatus addEncodedData(KSBONJSONEncodeContext* const context,
                                             const uint8_t* data,
                                             size_t length)
{
    return context->addEncodedData(data, length, context->userData);
}

static ksbonjson_encodeStatus encodeTypeCode(KSBONJSONEncodeContext* const context, uint8_t typeCode)
{
    return addEncodedData(context, &typeCode, 1);
}

static ksbonjson_encodeStatus encodeULEB128(KSBONJSONEncodeContext* const context, uint64_t value)
{
    // 64 bits / 7 bits per ULEB128 byte = max 10 bytes
    uint8_t buffer[10];
    
    unlikely_if(value == 0)
    {
        buffer[0] = 0;
        return addEncodedData(context, buffer, 1);
    }
    
    size_t index = 0;
    while(value != 0)
    {
        uint8_t encoded = (uint8_t)(value&0x7f);
        value >>= 7;
        if(value != 0)
        {
            encoded |= 0x80;
        }
        buffer[index++] = encoded;
    }
    return addEncodedData(context, buffer, index);
}

// Length up to 63 bits supported (1 bit is used for continuation)
static ksbonjson_encodeStatus encodeStringChunkHeader(KSBONJSONEncodeContext* const context,
                                                      uint64_t length,
                                                      bool isLastChunk)
{
    uint64_t value = length << 1;
    if(!isLastChunk)
    {
        value |= 1;
    }
    return encodeULEB128(context, value);
}

static ksbonjson_encodeStatus encodeStringChunk(KSBONJSONEncodeContext* const context,
                                                const char* restrict const data,
                                                size_t length,
                                                bool isLastChunk) {
    PROPAGATE_ERROR(encodeStringChunkHeader(context, length, isLastChunk));
    return addEncodedData(context, (uint8_t*)data, length);
}

static ksbonjson_encodeStatus encodeString(KSBONJSONEncodeContext* const context,
                                           const char* restrict const value,
                                           size_t valueLength)
{
    likely_if(valueLength < 32)
    {
        PROPAGATE_ERROR(encodeTypeCode(context, TYPE_STRINGSHORT + (uint8_t)valueLength));
        return addEncodedData(context, (uint8_t*)value, valueLength);
    }

    PROPAGATE_ERROR(encodeTypeCode(context, TYPE_STRINGLONG));
    return encodeStringChunk(context, value, valueLength, true);
}

static ksbonjson_encodeStatus encodeInt16(KSBONJSONEncodeContext* const context, int16_t value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
#   if KSBONJSON_USE_MEMCPY
        uint8_t data[sizeof(value)+1];
        data[0] = TYPE_INT16;
        memcpy(data+1, &value, sizeof(value));
        return addEncodedData(context, data, sizeof(data));
#   else
        union uint16_u u = {.u16 = (uint16_t)value};
        uint8_t data[] = {TYPE_INT16, u.b[0], u.b[1]};
        return addEncodedData(context, data, sizeof(data));
#   endif
#else
    union uint16_u u = {.u16 = (uint16_t)value};
    uint8_t data[] = {TYPE_INT16, u.b[1], u.b[0]};
    return addEncodedData(context, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeInt32(KSBONJSONEncodeContext* const context, int32_t value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
#   if KSBONJSON_USE_MEMCPY
        uint8_t data[sizeof(value)+1];
        data[0] = TYPE_INT32;
        memcpy(data+1, &value, sizeof(value));
        return addEncodedData(context, data, sizeof(data));
#   else
        union uint32_u u = {.u32 = (uint32_t)value};
        uint8_t data[] = {TYPE_INT32, u.b[0], u.b[1], u.b[2], u.b[3]};
        return addEncodedData(context, data, sizeof(data));
#   endif
#else
    union uint32_u u = {.u32 = (uint32_t)value};
    uint8_t data[] = {TYPE_INT32, u.b[3], u.b[2], u.b[1], u.b[0]};
    return addEncodedData(context, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeInt64(KSBONJSONEncodeContext* const context, int64_t value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
#   if KSBONJSON_USE_MEMCPY
        uint8_t data[sizeof(value)+1];
        data[0] = TYPE_INT64;
        memcpy(data+1, &value, sizeof(value));
        return addEncodedData(context, data, sizeof(data));
#   else
        union uint64_u u = {.u64 = (uint64_t)value};
        uint8_t data[] = {TYPE_INT64, u.b[0], u.b[1], u.b[2], u.b[3], u.b[4], u.b[5], u.b[6], u.b[7]};
        return addEncodedData(context, data, sizeof(data));
#   endif
#else
    union uint64_u u = {.u64 = (uint64_t)value};
    uint8_t data[] = {TYPE_INT64, u.b[7], u.b[6], u.b[5], u.b[4], u.b[3], u.b[2], u.b[1], u.b[0]};
    return addEncodedData(context, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeUInt64(KSBONJSONEncodeContext* const context, uint64_t value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
#   if KSBONJSON_USE_MEMCPY
        uint8_t data[sizeof(value)+1];
        data[0] = TYPE_UINT64;
        memcpy(data+1, &value, sizeof(value));
        return addEncodedData(context, data, sizeof(data));
#   else
        union uint64_u u = {.u64 = value};
        uint8_t data[] = {TYPE_UINT64, u.b[0], u.b[1], u.b[2], u.b[3], u.b[4], u.b[5], u.b[6], u.b[7]};
        return addEncodedData(context, data, sizeof(data));
#   endif
#else
    union uint64_u u = {.u64 = value};
    uint8_t data[] = {TYPE_UINT64, u.b[7], u.b[6], u.b[5], u.b[4], u.b[3], u.b[2], u.b[1], u.b[0]};
    return addEncodedData(context, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeFloat32(KSBONJSONEncodeContext* const context, float value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
#   if KSBONJSON_USE_MEMCPY
        uint8_t data[sizeof(value)+1];
        data[0] = TYPE_FLOAT32;
        memcpy(data+1, &value, sizeof(value));
        return addEncodedData(context, data, sizeof(data));
#   else
        union float32_u u = {.f32 = value};
        uint8_t data[] = {TYPE_FLOAT32, u.b[0], u.b[1], u.b[2], u.b[3]};
        return addEncodedData(context, data, sizeof(data));
#   endif
#else
    union float32_u u = {.f32 = value};
    uint8_t data[] = {TYPE_FLOAT32, u.b[3], u.b[2], u.b[1], u.b[0]};
    return addEncodedData(context, data, sizeof(data));
#endif
}

static ksbonjson_encodeStatus encodeFloat64(KSBONJSONEncodeContext* const context, double value)
{
#if KSBONJSON_IS_LITTLE_ENDIAN
#   if KSBONJSON_USE_MEMCPY
        uint8_t data[sizeof(value)+1];
        data[0] = TYPE_FLOAT64;
        memcpy(data+1, &value, sizeof(value));
        return addEncodedData(context, data, sizeof(data));
#   else
        union float64_u u = {.f64 = value};
        uint8_t data[] = {TYPE_FLOAT64, u.b[0], u.b[1], u.b[2], u.b[3], u.b[4], u.b[5], u.b[6], u.b[7]};
        return addEncodedData(context, data, sizeof(data));
#   endif
#else
    union float64_u u = {.f64 = value};
    uint8_t data[] = {TYPE_FLOAT64, u.b[7], u.b[6], u.b[5], u.b[4], u.b[3], u.b[2], u.b[1], u.b[0]};
    return addEncodedData(context, data, sizeof(data));
#endif
}

#if KSBONJSON_OPTIMIZE_SPACE
static ksbonjson_encodeStatus encodeBigInt(KSBONJSONEncodeContext* const context,
                                           uint8_t typeCode,
                                           uint64_t value)
{
    // Assumption: We never call this function with a value of 0

    uint8_t data[10] =
    {
        typeCode, // Type code
        0,        // Length field (will only ever be 1 byte of ULEB128 data)
                  // Data (up to 8 bytes in this case)
    };
    static const int data_offset = 2;
    size_t index = data_offset;
    while(value != 0)
    {
        data[index++] = (uint8_t)(value&0xff);
        value >>= 8;
    }
    data[1] = (uint8_t)(index-data_offset) << 2; // Length field, no exponent

    return addEncodedData(context, data, index);
}

#define MASK_OUT_LOWER_40_BITS 0xffffff0000000000
#define MASK_OUT_LOWER_39_BITS 0xffffff8000000000
#define MASK_OUT_LOWER_48_BITS 0xffff000000000000
#define MASK_OUT_LOWER_47_BITS 0xffff800000000000

#define TRY_ENCODE_BIGINT(BITS, FROM_VALUE) \
    do \
    { \
        if(((FROM_VALUE)&(int64_t)MASK_OUT_LOWER_ ## BITS ## _BITS) == (int64_t)MASK_OUT_LOWER_ ## BITS ## _BITS || \
        ((FROM_VALUE)&MASK_OUT_LOWER_ ## BITS ## _BITS) == 0) \
        { \
            uint8_t typeCode = TYPE_BIGPOSITIVE; \
            uint64_t toValue = (uint64_t)(FROM_VALUE); \
            if((FROM_VALUE) < 0) \
            { \
                typeCode = TYPE_BIGNEGATIVE; \
                toValue = (uint64_t)(-(FROM_VALUE)); \
            } \
            return encodeBigInt(context, typeCode, toValue); \
        } \
    } \
    while(0)

#define TRY_ENCODE_BIGINT_UNSIGNED(BITS, FROM_VALUE) \
    do \
    { \
        if(((FROM_VALUE)&MASK_OUT_LOWER_ ## BITS ## _BITS) == 0) \
        { \
            return encodeBigInt(context, TYPE_BIGPOSITIVE, FROM_VALUE); \
        } \
    } \
    while(0)
#else
#define TRY_ENCODE_BIGINT(BITS, VALUE)
#define TRY_ENCODE_BIGINT_UNSIGNED(BITS, VALUE)
#endif

#define TRY_ENCODE_SMALLINT(FROM_VALUE, FROM_TYPE) \
    do \
    { \
        int8_t toValue = (int8_t)(FROM_VALUE); \
        if((FROM_TYPE)toValue == (FROM_VALUE)) \
        { \
            likely_if((toValue >= 0 && toValue <= SMALLINT_MAX) || \
                      (toValue >= SMALLINT_MIN && toValue <= -1)) \
            { \
                return encodeTypeCode(context, (uint8_t)toValue); \
            } \
            else \
            { \
                return encodeInt16(context, toValue); \
            } \
        } \
    } \
    while(0)

#define TRY_ENCODE_SMALLINT_UNSIGNED(FROM_VALUE, FROM_TYPE) \
    do \
    { \
        int8_t toValue = (int8_t)(FROM_VALUE); \
        if(toValue >= 0 && (FROM_TYPE)toValue == (FROM_VALUE)) \
        { \
            likely_if((toValue >= 0 && toValue <= SMALLINT_MAX) || \
                      (toValue >= SMALLINT_MIN && toValue <= -1)) \
            { \
                return encodeTypeCode(context, (uint8_t)toValue); \
            } \
            else \
            { \
                return encodeInt16(context, toValue); \
            } \
        } \
    } \
    while(0)

#define TRY_ENCODE_INT(TO_SIZE, FROM_VALUE, FROM_TYPE) \
    do \
    { \
        int ## TO_SIZE ## _t toValue = (int ## TO_SIZE ## _t)(FROM_VALUE); \
        if((FROM_TYPE)toValue == (FROM_VALUE)) \
        { \
            return encodeInt ## TO_SIZE (context, toValue); \
        } \
    } \
    while(0)

#define TRY_ENCODE_INT_UNSIGNED(SIZE, FROM_VALUE, FROM_TYPE) \
    do \
    { \
        int ## SIZE ## _t toValue = (int ## SIZE ## _t)(FROM_VALUE); \
        if(toValue >= 0 && (FROM_TYPE)toValue == (FROM_VALUE)) \
        { \
            return encodeInt ## SIZE (context, toValue); \
        } \
    } \
    while(0)

#define TRY_ENCODE_F32(FROM_VALUE, FROM_TYPE) \
    do \
    { \
        float toValue = (float)(FROM_VALUE); \
        if((FROM_TYPE)toValue == (FROM_VALUE)) \
        { \
            return encodeFloat32(context, toValue); \
        } \
    } \
    while(0)


// ============================================================================
// API
// ============================================================================

void ksbonjson_beginEncode(KSBONJSONEncodeContext* const context,
                           KSBONJSONAddEncodedDataFunc addEncodedDataFunc,
                           void* const userData)
{
    *context = (KSBONJSONEncodeContext){0};
    context->addEncodedData = addEncodedDataFunc;
    context->userData = userData;
}

ksbonjson_encodeStatus ksbonjson_endEncode(KSBONJSONEncodeContext* context)
{
    unlikely_if(context->containerDepth > 0)
    {
        return KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN;
    }
    unlikely_if(context->containers[context->containerDepth].isChunkingString)
    {
        return KSBONJSON_ENCODE_CHUNKING_STRING;
    }
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_terminateDocument(KSBONJSONEncodeContext* context)
{
    while(context->containerDepth > 0)
    {
        PROPAGATE_ERROR(ksbonjson_endContainer(context));
    }
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_addBoolean(KSBONJSONEncodeContext* context, bool value)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    state->isExpectingName = true;
    return encodeTypeCode(context, value ? TYPE_TRUE : TYPE_FALSE);
}


ksbonjson_encodeStatus ksbonjson_addFloat(KSBONJSONEncodeContext* context, double value)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    state->isExpectingName = true;
    {
        int64_t asInt64 = (int64_t)value;
        if((double)asInt64 == value) {
            return ksbonjson_addInteger(context, asInt64);
        }
    }
    TRY_ENCODE_F32(value, double);
    return encodeFloat64(context, value);
}

ksbonjson_encodeStatus ksbonjson_addInteger(KSBONJSONEncodeContext* context, int64_t value)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    state->isExpectingName = true;
    TRY_ENCODE_SMALLINT(value, int64_t); // 1 byte
    TRY_ENCODE_INT(16, value, int64_t);  // 3 bytes
    TRY_ENCODE_INT(32, value, int64_t);  // 5 bytes
    TRY_ENCODE_BIGINT(39, value);        // 7 bytes (if enabled)
    TRY_ENCODE_BIGINT(47, value);        // 8 bytes (if enabled)
    return encodeInt64(context, value);  // 9 bytes
}

ksbonjson_encodeStatus ksbonjson_addUInteger(KSBONJSONEncodeContext* context, uint64_t value)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    state->isExpectingName = true;
    TRY_ENCODE_SMALLINT_UNSIGNED(value, uint64_t); // 1 byte
    TRY_ENCODE_INT_UNSIGNED(16, value, uint64_t);  // 3 bytes
    TRY_ENCODE_INT_UNSIGNED(32, value, uint64_t);  // 5 bytes
    TRY_ENCODE_BIGINT_UNSIGNED(40, value);         // 7 bytes (if enabled)
    TRY_ENCODE_BIGINT_UNSIGNED(48, value);         // 8 bytes (if enabled)
    return encodeUInt64(context, value);           // 9 bytes
}

ksbonjson_encodeStatus ksbonjson_addNull(KSBONJSONEncodeContext* context)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    state->isExpectingName = true;
    return encodeTypeCode(context, TYPE_NULL);
}

ksbonjson_encodeStatus ksbonjson_addString(KSBONJSONEncodeContext* context,
                                           const char* value,
                                           size_t valueLength)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_CHUNKING_STRING();
    SHOULD_NOT_BE_NULL(value);

    state->isExpectingName = !state->isExpectingName;
    return encodeString(context, value, valueLength);
}

ksbonjson_encodeStatus ksbonjson_chunkString(KSBONJSONEncodeContext* context,
                                             const char* chunk,
                                             size_t chunkLength,
                                             bool isLastChunk)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_NULL(chunk);

    if(!state->isChunkingString)
    {
        PROPAGATE_ERROR(encodeTypeCode(context, TYPE_STRINGLONG));
    }
    state->isChunkingString = !isLastChunk;
    if(isLastChunk)
    {
        state->isExpectingName = !state->isExpectingName;
    }

    return encodeStringChunk(context, chunk, chunkLength, isLastChunk);
}

ksbonjson_encodeStatus ksbonjson_addBONJSONDocument(KSBONJSONEncodeContext* context,
                                                    const uint8_t* bonjsonDocument,
                                                    size_t documentLength)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    state->isExpectingName = true;
    return addEncodedData(context, bonjsonDocument, documentLength);
}

ksbonjson_encodeStatus ksbonjson_beginObject(KSBONJSONEncodeContext* context)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    PROPAGATE_ERROR(encodeTypeCode(context, TYPE_OBJECT));
    context->containerDepth++;
    context->containers[context->containerDepth] = (KSBONJSONContainerState)
    {
        .isInObject = true,
        .isExpectingName = true,
    };
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_beginArray(KSBONJSONEncodeContext* context)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    PROPAGATE_ERROR(encodeTypeCode(context, TYPE_ARRAY));
    context->containerDepth++;
    context->containers[context->containerDepth] = (KSBONJSONContainerState)
    {
        .isInObject = false,
    };
    return KSBONJSON_ENCODE_OK;
}

ksbonjson_encodeStatus ksbonjson_endContainer(KSBONJSONEncodeContext* context)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_VALUE();
    SHOULD_NOT_BE_CHUNKING_STRING();
    unlikely_if(context->containerDepth <= 0)
    {
        return KSBONJSON_ENCODE_CLOSED_TOO_MANY_CONTAINERS;
    }

    PROPAGATE_ERROR(encodeTypeCode(context, TYPE_END));
    context->containerDepth--;
    state = &context->containers[context->containerDepth];
    state->isExpectingName = true; // Update parent container's state.
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
            return "addEncodedData() failed to process the passed in data";
        default:
            return "(unknown status - was it a user-defined status code?)";
    }
}
