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
#pragma mark - Helpers -
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

#define SMALLINT_MAX 105
#define SMALLINT_MIN -106

union uint16_u
{
    uint16_t u16;
    uint8_t bytes[2];
};

union float32_u
{
    float f32;
    uint32_t u32;
    uint8_t bytes[4];
};

union float64_u
{
    double f64;
    uint64_t u64;
    uint8_t bytes[8];
};


// ============================================================================
#pragma mark - Implementation -
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
    uint8_t data[3] =
    {
        TYPE_INT16,
        (uint8_t)(value&0xff),
        (uint8_t)((value>>8)&0xff),
    };
    return addEncodedData(context, data, sizeof(data));
}

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

static ksbonjson_encodeStatus encodeFloat32(KSBONJSONEncodeContext* const context, float value)
{
    union float32_u u =
    {
        .f32 = value,
    };
    uint8_t data[] =
    {
        TYPE_FLOAT32,
        (uint8_t)(u.u32&0xff),
        (uint8_t)((u.u32>>8)&0xff),
        (uint8_t)((u.u32>>16)&0xff),
        (uint8_t)((u.u32>>24)&0xff),
    };
    return addEncodedData(context, data, sizeof(data));
}

static ksbonjson_encodeStatus encodeFloat64(KSBONJSONEncodeContext* const context, double value)
{
    union float64_u u =
    {
        .f64 = value,
    };
    uint8_t data[] =
    {
        TYPE_FLOAT32,
        (uint8_t)(u.u64&0xff),
        (uint8_t)((u.u64>>8)&0xff),
        (uint8_t)((u.u64>>16)&0xff),
        (uint8_t)((u.u64>>24)&0xff),
        (uint8_t)((u.u64>>32)&0xff),
        (uint8_t)((u.u64>>40)&0xff),
        (uint8_t)((u.u64>>48)&0xff),
        (uint8_t)((u.u64>>56)&0xff),
    };
    return addEncodedData(context, data, sizeof(data));
}

#define TRY_ENCODE_I8(VALUE, FROM_TYPE) \
    do \
    { \
        int8_t i8Val = (int8_t)(VALUE); \
        if((FROM_TYPE)i8Val == (VALUE)) \
        { \
            likely_if((i8Val >= 0 && i8Val <= SMALLINT_MAX) || (i8Val >= SMALLINT_MIN && i8Val <= -1)) \
            { \
                return encodeTypeCode(context, (uint8_t)i8Val); \
            } \
            else \
            { \
                return encodeInt16(context, i8Val); \
            } \
        } \
    } \
    while(0)

#define TRY_ENCODE_I16(VALUE, FROM_TYPE) \
    do \
    { \
        int16_t i16Val = (int16_t)(VALUE); \
        if((FROM_TYPE)i16Val == (VALUE)) \
        { \
            return encodeInt16(context, i16Val); \
        } \
    } \
    while(0)

#define TRY_ENCODE_F32(VALUE, FROM_TYPE) \
    do \
    { \
        float f32Val = (float)(VALUE); \
        if((FROM_TYPE)f32Val == (VALUE)) \
        { \
            return encodeFloat32(context, f32Val); \
        } \
    } \
    while(0)


// ============================================================================
#pragma mark - API -
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
        return KSBONJSON_ENCODE_UNBALANCED_CONTAINERS;
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

ksbonjson_encodeStatus ksbonjson_addName(KSBONJSONEncodeContext* context,
                                         const char* name,
                                         size_t nameLength) {
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_BE_IN_OBJECT();
    SHOULD_NOT_BE_EXPECTING_OBJECT_VALUE();
    SHOULD_NOT_BE_CHUNKING_STRING();
    SHOULD_NOT_BE_NULL(name);

    state->isExpectingName = false;
    return encodeString(context, name, nameLength);
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
    TRY_ENCODE_I8(value, double);
    TRY_ENCODE_I16(value, double);
    TRY_ENCODE_F32(value, double);
    return encodeFloat64(context, value);
}

ksbonjson_encodeStatus ksbonjson_addInteger(KSBONJSONEncodeContext* context, int64_t value)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    state->isExpectingName = true;
    TRY_ENCODE_I8(value, int64_t);
    TRY_ENCODE_I16(value, int64_t);
    TRY_ENCODE_F32(value, int64_t);
    uint8_t typeCode = TYPE_BIGPOSITIVE;
    uint64_t u64Val = (uint64_t)value;
    if(value < 0)
    {
        typeCode = TYPE_BIGNEGATIVE;
        u64Val = (uint64_t)(-value);
    }
    return encodeBigInt(context, typeCode, u64Val);
}

ksbonjson_encodeStatus ksbonjson_addUInteger(KSBONJSONEncodeContext* context, uint64_t value)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();

    state->isExpectingName = true;
    TRY_ENCODE_I8(value, uint64_t);
    TRY_ENCODE_I16(value, uint64_t);
    TRY_ENCODE_F32(value, uint64_t);
    return encodeBigInt(context, TYPE_BIGPOSITIVE, value);
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
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_CHUNKING_STRING();
    SHOULD_NOT_BE_NULL(value);

    state->isExpectingName = true;
    return encodeString(context, value, valueLength);
}

ksbonjson_encodeStatus ksbonjson_chunkString(KSBONJSONEncodeContext* context,
                                             const char* chunk,
                                             size_t chunkLength,
                                             bool isLastChunk)
{
    KSBONJSONContainerState* state = &context->containers[context->containerDepth];
    SHOULD_NOT_BE_EXPECTING_OBJECT_NAME();
    SHOULD_NOT_BE_NULL(chunk);

    if(!state->isChunkingString)
    {
        PROPAGATE_ERROR(encodeTypeCode(context, TYPE_STRINGLONG));
    }
    state->isChunkingString = !isLastChunk;
    state->isExpectingName = true;

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
        return KSBONJSON_ENCODE_UNBALANCED_CONTAINERS;
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
            return "Expected an object element name, but got a value instead";
        case KSBONJSON_ENCODE_EXPECTED_OBJECT_VALUE:
            return "Expected an object element value, but got a name instead";
        case KSBONJSON_ENCODE_CHUNKING_STRING:
            return "Attempted to add a discrete value while chunking a string";
        case KSBONJSON_ENCODE_NULL_POINTER:
            return "Passed in a NULL pointer where one is not allowed";
        case KSBONJSON_ENCODE_UNBALANCED_CONTAINERS:
            return "Either too many or not enough containers were closed";
        case KSBONJSON_ENCODE_NOT_IN_AN_OBJECT:
            return "Attempted to set an object element name while not in an object";
        case KSBONJSON_ENCODE_COULD_NOT_ADD_DATA:
            return "addEncodedData() failed to process the passed in data";
        default:
            return "(unknown status - was it a user-defined status code?)";
    }
}
