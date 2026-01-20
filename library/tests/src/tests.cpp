//
//  KSBONJSONCodec.h
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

#include <gtest/gtest.h>
#include <math.h>
#include <algorithm>

#include <ksbonjson/KSBONJSONEncoder.h>
#include <ksbonjson/KSBONJSONDecoder.h>

#include "encoder.h"
#include "decoder.h"
#include "events.h"


#define REPORT_DECODING false
#define REPORT_ENCODING false


#define MARK_UNUSED(x) (void)(x)


class Decoder: public ksbonjson::Decoder
{
public:
    virtual ~Decoder() {}

public:
    std::vector<std::shared_ptr<Event>> events;

protected:
    ksbonjson_decodeStatus onValue(bool value) override
    {
        addEvent(std::make_shared<BooleanEvent>(value));
        return KSBONJSON_DECODE_OK;
    }

    ksbonjson_decodeStatus onValue(int64_t value) override
    {
        addEvent(std::make_shared<IntegerEvent>(value));
        return KSBONJSON_DECODE_OK;
    }

    ksbonjson_decodeStatus onValue(uint64_t value) override
    {
        addEvent(std::make_shared<UIntegerEvent>(value));
        return KSBONJSON_DECODE_OK;
    }

    ksbonjson_decodeStatus onValue(double value) override
    {
        addEvent(std::make_shared<FloatEvent>(value));
        return KSBONJSON_DECODE_OK;
    }

    ksbonjson_decodeStatus onValue(KSBigNumber value) override
    {
        addEvent(std::make_shared<BigNumberEvent>(value));
        return KSBONJSON_DECODE_OK;
    }

    ksbonjson_decodeStatus onNull() override
    {
        addEvent(std::make_shared<NullEvent>());
        return KSBONJSON_DECODE_OK;
    }

    ksbonjson_decodeStatus onString(const char* value, size_t length) override
    {
        addEvent(std::make_shared<StringEvent>(value, length));
        return KSBONJSON_DECODE_OK;
    }

    ksbonjson_decodeStatus onStringChunk(const char* value, size_t length, bool isLastChunk) override
    {
        addEvent(std::make_shared<StringChunkEvent>(value, length, isLastChunk ? CHUNK_LAST : CHUNK_HAS_NEXT));
        return KSBONJSON_DECODE_OK;
    }

    ksbonjson_decodeStatus onBeginObject(size_t elementCountHint) override
    {
        addEvent(std::make_shared<ObjectBeginEvent>(elementCountHint));
        return KSBONJSON_DECODE_OK;
    }

    ksbonjson_decodeStatus onBeginArray(size_t elementCountHint) override
    {
        addEvent(std::make_shared<ArrayBeginEvent>(elementCountHint));
        return KSBONJSON_DECODE_OK;
    }

    ksbonjson_decodeStatus onEndContainer() override
    {
        addEvent(std::make_shared<ContainerEndEvent>());
        return KSBONJSON_DECODE_OK;
    }

    ksbonjson_decodeStatus onEndData() override
    {
        return KSBONJSON_DECODE_OK;
    }

private:
    void addEvent(std::shared_ptr<Event> event)
    {
        if(REPORT_DECODING)
        {
            printf("%s\n", event->description().c_str());
        }
        events.push_back(event);
    }
};

class Encoder: public ksbonjson::Encoder
{
public:
    Encoder(size_t bufferSize)
    : buffer(bufferSize)
    {}
    virtual ~Encoder() {}

    std::vector<uint8_t> get()
    {
        return std::vector<uint8_t>(buffer.begin(), buffer.begin()+index);
    }

    void reset()
    {
        index = 0;
    }
protected:
    ksbonjson_encodeStatus addEncodedData(const uint8_t* data, size_t length) override
    {
        if(REPORT_ENCODING)
        {
            static const char hexDigits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
            for(size_t i = 0; i < length; i++)
            {
                uint8_t ch = data[i];
                uint8_t hi = ch >> 4;
                uint8_t lo = ch & 15;
                printf("%c%c ", hexDigits[hi], hexDigits[lo]);
            }
            printf("\n");
        }

        if(index + length > buffer.size())
        {
            return KSBONJSON_ENCODE_COULD_NOT_ADD_DATA;
        }
        std::copy(data, data+length, buffer.begin()+index);
        index += length;
        return KSBONJSON_ENCODE_OK;
    }
private:
    std::vector<uint8_t> buffer;
    size_t index{0};
};

class FailingEncoder: public ksbonjson::Encoder
{
public:
    FailingEncoder() {}
    virtual ~FailingEncoder() {}
protected:
    ksbonjson_encodeStatus addEncodedData(const uint8_t* data, size_t length) override
    {
        MARK_UNUSED(data);
        MARK_UNUSED(length);
        return KSBONJSON_ENCODE_COULD_NOT_ADD_DATA;
    }
};

// ============================================================================
// Encoder Context
// ============================================================================

class EncoderContext
{
public:
    EncoderContext(size_t size)
    : buffer(size)
    {}

    ksbonjson_encodeStatus add(const uint8_t* data, size_t length)
    {
        if(REPORT_ENCODING)
        {
            static const char hexDigits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
            for(size_t i = 0; i < length; i++)
            {
                uint8_t ch = data[i];
                uint8_t hi = ch >> 4;
                uint8_t lo = ch & 15;
                printf("%c%c ", hexDigits[hi], hexDigits[lo]);
            }
            printf("\n");
        }

        if(index + length > buffer.size())
        {
            return KSBONJSON_ENCODE_COULD_NOT_ADD_DATA;
        }
        std::copy(data, data+length, buffer.begin()+index);
        index += length;
        return KSBONJSON_ENCODE_OK;
    }

    std::vector<uint8_t> get()
    {
        return std::vector<uint8_t>(buffer.begin(), buffer.begin()+index);
    }

    void reset()
    {
        index = 0;
    }

private:
    std::vector<uint8_t> buffer;
    int index{0};
};

ksbonjson_encodeStatus addEncodedDataCallback(const uint8_t* KSBONJSON_RESTRICT data,
                                              size_t dataLength,
                                              void* KSBONJSON_RESTRICT userData)
{
    EncoderContext* ctx = (EncoderContext*)userData;
    return ctx->add(data, dataLength);
}

ksbonjson_encodeStatus addEncodedDataFailCallback(const uint8_t* KSBONJSON_RESTRICT data,
                                                  size_t dataLength,
                                                  void* KSBONJSON_RESTRICT userData)
{
    MARK_UNUSED(data);
    MARK_UNUSED(dataLength);
    MARK_UNUSED(userData);
    return KSBONJSON_ENCODE_COULD_NOT_ADD_DATA;
}


// ============================================================================
// Test Support
// ============================================================================

void assert_events_equal(std::vector<std::shared_ptr<Event>> expected, std::vector<std::shared_ptr<Event>> actual)
{
    bool isEqual = true;
    if(expected.size() == actual.size())
    {
        for(size_t i = 0; i < expected.size(); i++)
        {
            if(*expected[i] != *actual[i])
            {
                isEqual = false;
                break;
            }
        }
    }
    else
    {
        isEqual = false;
    }
    if(!isEqual)
    {
        std::cout << expected << " != " << actual << std::endl;
        ADD_FAILURE();
    }
}

void assert_encode_decode(std::vector<std::shared_ptr<Event>> events, std::vector<uint8_t> expected_encoded)
{
    if(REPORT_ENCODING || REPORT_DECODING)
    {
        printf("\n[assert_encode_decode]\n");
    }

    // Encode
    if(REPORT_ENCODING)
    {
        printf("\n[assert_encode_decode]: Encode\n");
    }
    KSBONJSONEncodeContext eContext;
    EncoderContext eCtx(10000);
    ksbonjson_beginEncode(&eContext, addEncodedDataCallback, &eCtx);
    for (const std::shared_ptr<Event>& event: events)
    {
        ASSERT_EQ(KSBONJSON_ENCODE_OK, (*event)(&eContext));
    }
    ASSERT_EQ(KSBONJSON_ENCODE_OK, ksbonjson_endEncode(&eContext));
    std::vector<uint8_t> actual_encoded = eCtx.get();
    ASSERT_EQ(expected_encoded, actual_encoded);

    // Decode
    if(REPORT_DECODING)
    {
        printf("\n[assert_encode_decode]: Decode\n");
    }
    Decoder decoder;
    size_t decodedOffset = 0;
    std::vector<uint8_t> document = actual_encoded;
    ASSERT_EQ(KSBONJSON_DECODE_OK, decoder.decode(document, &decodedOffset));
    ASSERT_EQ(expected_encoded.size(), decodedOffset);
    assert_events_equal(events, decoder.events);

    // Encode again
    if(REPORT_ENCODING)
    {
        printf("\n[assert_encode_decode]: Encode again\n");
    }
    eCtx.reset();
    ksbonjson_beginEncode(&eContext, addEncodedDataCallback, &eCtx);
    for (const std::shared_ptr<Event>& event: decoder.events)
    {
        ASSERT_EQ(KSBONJSON_ENCODE_OK, (*event)(&eContext));
    }
    ASSERT_EQ(KSBONJSON_ENCODE_OK, ksbonjson_endEncode(&eContext));
    actual_encoded = eCtx.get();
    ASSERT_EQ(expected_encoded, actual_encoded);
}

void assert_encode(std::vector<std::shared_ptr<Event>> events, std::vector<uint8_t> expected_encoded)
{
    if(REPORT_ENCODING)
    {
        printf("\n[assert_encode]\n");
    }

    // Encode
    if(REPORT_ENCODING)
    {
        printf("\n[assert_encode]: Encode\n");
    }
    KSBONJSONEncodeContext eContext;
    EncoderContext eCtx(10000);
    ksbonjson_beginEncode(&eContext, addEncodedDataCallback, &eCtx);
    for (const std::shared_ptr<Event>& event: events)
    {
        ASSERT_EQ(KSBONJSON_ENCODE_OK, (*event)(&eContext));
    }
    ASSERT_EQ(KSBONJSON_ENCODE_OK, ksbonjson_endEncode(&eContext));
    std::vector<uint8_t> actual_encoded = eCtx.get();
    ASSERT_EQ(expected_encoded, actual_encoded);
}

void assert_decode(std::vector<std::shared_ptr<Event>> expected_events, std::vector<uint8_t> document)
{
    if(REPORT_ENCODING || REPORT_DECODING)
    {
        printf("\n[assert_decode]\n");
    }

    Decoder decoder;
    size_t decodedOffset = 0;
    ASSERT_EQ(KSBONJSON_DECODE_OK, decoder.decode(document, &decodedOffset));
    ASSERT_EQ(document.size(), decodedOffset);
    assert_events_equal(expected_events, decoder.events);
}

void assert_encode_result(ksbonjson_encodeStatus expectedResult, std::vector<std::shared_ptr<Event>> events)
{
    if(REPORT_ENCODING)
    {
        printf("\n[assert_encode_result]\n");
    }

    KSBONJSONEncodeContext eContext;
    memset(&eContext, 0, sizeof(eContext));
    EncoderContext eCtx(10000);
    ksbonjson_beginEncode(&eContext, addEncodedDataCallback, &eCtx);
    for (const std::shared_ptr<Event>& event: events)
    {
        ksbonjson_encodeStatus result = (*event)(&eContext);
        if(result == expectedResult)
        {
            SUCCEED();
            return;
        }
        if(result != KSBONJSON_ENCODE_OK)
        {
            ASSERT_EQ(expectedResult, result);
        }
    }

    ASSERT_EQ(expectedResult, ksbonjson_endEncode(&eContext));
}

void assert_decode_result(ksbonjson_decodeStatus expectedResult, std::vector<uint8_t> document)
{
    if(REPORT_DECODING)
    {
        printf("\n[assert_decode_result]\n");
    }

    Decoder decoder;
    size_t decodedOffset = 0;
    ASSERT_EQ(expectedResult, decoder.decode(document, &decodedOffset));
}

// ============================================================================
// Tests
// ============================================================================

// New spec type codes:
// Small integers: 0x00-0xc8 encode values -100 to 100 (value = type_code - 100)
// Unsigned integers: 0xd0-0xd7 (1-8 bytes)
// Signed integers: 0xd8-0xdf (1-8 bytes)
// Short strings: 0xe0-0xef (0-15 bytes)
// Long string: 0xf0
// Big number: 0xf1, Float16/32/64: 0xf2-0xf4, Null/False/True: 0xf5-0xf7
// Array/Object: 0xf8-0xf9 (followed by chunk header, no TYPE_END)
enum
{
    // Unsigned integers: 0xd0-0xd7
    TYPE_UINT8  = 0xd0,
    TYPE_UINT16 = 0xd1,
    TYPE_UINT24 = 0xd2,
    TYPE_UINT32 = 0xd3,
    TYPE_UINT40 = 0xd4,
    TYPE_UINT48 = 0xd5,
    TYPE_UINT56 = 0xd6,
    TYPE_UINT64 = 0xd7,

    // Signed integers: 0xd8-0xdf
    TYPE_SINT8  = 0xd8,
    TYPE_SINT16 = 0xd9,
    TYPE_SINT24 = 0xda,
    TYPE_SINT32 = 0xdb,
    TYPE_SINT40 = 0xdc,
    TYPE_SINT48 = 0xdd,
    TYPE_SINT56 = 0xde,
    TYPE_SINT64 = 0xdf,

    // Short strings: 0xe0-0xef
    TYPE_STRING0  = 0xe0,
    TYPE_STRING1  = 0xe1,
    TYPE_STRING2  = 0xe2,
    TYPE_STRING3  = 0xe3,
    TYPE_STRING4  = 0xe4,
    TYPE_STRING5  = 0xe5,
    TYPE_STRING6  = 0xe6,
    TYPE_STRING7  = 0xe7,
    TYPE_STRING8  = 0xe8,
    TYPE_STRING9  = 0xe9,
    TYPE_STRING10 = 0xea,
    TYPE_STRING11 = 0xeb,
    TYPE_STRING12 = 0xec,
    TYPE_STRING13 = 0xed,
    TYPE_STRING14 = 0xee,
    TYPE_STRING15 = 0xef,

    // Long string: 0xf0
    TYPE_STRING = 0xf0,

    // Other types
    TYPE_BIG_NUMBER = 0xf1,
    TYPE_FLOAT16    = 0xf2,
    TYPE_FLOAT32    = 0xf3,
    TYPE_FLOAT64    = 0xf4,
    TYPE_NULL       = 0xf5,
    TYPE_FALSE      = 0xf6,
    TYPE_TRUE       = 0xf7,
    TYPE_ARRAY      = 0xf8,
    TYPE_OBJECT     = 0xf9,
};

enum
{
    SMALLINT_MIN = -100,
    SMALLINT_MAX = 100,
    SMALLINT_BIAS = 100,  // type_code = value + SMALLINT_BIAS
};

// Helper to convert small int value to type code
#define SMALLINT(v) ((uint8_t)((v) + SMALLINT_BIAS))

// Helper to create chunk header for containers
// Format: ((count << 1) | continuation) << 1, plus trailing 1s terminated by 0
// For small counts with no continuation: count << 2
#define CHUNK(count) ((uint8_t)((count) << 2))
#define CHUNK_CONT(count) ((uint8_t)(((count) << 2) | 2))

// ------------------------------------
// Basic Tests
// ------------------------------------

TEST(EncodeDecode, null)
{
    assert_encode_decode({std::make_shared<NullEvent>()}, {TYPE_NULL});
}

TEST(EncodeDecode, boolean)
{
    assert_encode_decode({std::make_shared<BooleanEvent>(true)}, {TYPE_TRUE});
    assert_encode_decode({std::make_shared<BooleanEvent>(false)}, {TYPE_FALSE});
}

TEST(EncodeDecode, float16)
{
    assert_encode_decode({std::make_shared<FloatEvent>(1.125)}, {TYPE_FLOAT16, 0x90, 0x3f});
}

TEST(EncodeDecode, float32)
{
    assert_encode_decode({std::make_shared<FloatEvent>(0x1.3f7p5)}, {TYPE_FLOAT32, 0x00, 0xb8, 0x1f, 0x42});

    assert_decode({std::make_shared<FloatEvent>(1.125)}, {TYPE_FLOAT32, 0x00, 0x00, 0x90, 0x3f});
}

TEST(EncodeDecode, float64)
{
    assert_encode_decode({std::make_shared<FloatEvent>(1.234)}, {TYPE_FLOAT64, 0x58, 0x39, 0xb4, 0xc8, 0x76, 0xbe, 0xf3, 0x3f});

    assert_decode({std::make_shared<FloatEvent>(1.125)}, {TYPE_FLOAT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf2, 0x3f});
}

TEST(EncodeDecode, smallint)
{
    // Small integers: value = type_code - 100, so type_code = value + 100
    assert_encode_decode({std::make_shared<IntegerEvent>( 100)}, {SMALLINT( 100)});  // 0xc8
    assert_encode_decode({std::make_shared<IntegerEvent>(  10)}, {SMALLINT(  10)});  // 0x6e
    assert_encode_decode({std::make_shared<IntegerEvent>(   0)}, {SMALLINT(   0)});  // 0x64
    assert_encode_decode({std::make_shared<IntegerEvent>(   1)}, {SMALLINT(   1)});  // 0x65
    assert_encode_decode({std::make_shared<IntegerEvent>(  -1)}, {SMALLINT(  -1)});  // 0x63
    assert_encode_decode({std::make_shared<IntegerEvent>( -60)}, {SMALLINT( -60)});  // 0x28
    assert_encode_decode({std::make_shared<IntegerEvent>(-100)}, {SMALLINT(-100)});  // 0x00
}

TEST(EncodeDecode, int8)
{
    assert_encode_decode({std::make_shared<IntegerEvent>( 127)}, {TYPE_SINT8, (uint8_t)( 127)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 126)}, {TYPE_SINT8, (uint8_t)( 126)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 122)}, {TYPE_SINT8, (uint8_t)( 122)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 101)}, {TYPE_SINT8, (uint8_t)( 101)});

    assert_encode_decode({std::make_shared<IntegerEvent>( 128)}, {TYPE_UINT8, (uint8_t)( 128)}); 
    assert_encode_decode({std::make_shared<IntegerEvent>( 200)}, {TYPE_UINT8, (uint8_t)( 200)}); 
    assert_encode_decode({std::make_shared<IntegerEvent>( 255)}, {TYPE_UINT8, (uint8_t)( 255)}); 

    assert_encode_decode({std::make_shared<IntegerEvent>(-101)}, {TYPE_SINT8, (uint8_t)(-101)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-121)}, {TYPE_SINT8, (uint8_t)(-121)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-123)}, {TYPE_SINT8, (uint8_t)(-123)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-127)}, {TYPE_SINT8, (uint8_t)(-127)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-128)}, {TYPE_SINT8, (uint8_t)(-128)});

    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_SINT8, 50});
    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_UINT8, 50});
    assert_decode({std::make_shared<IntegerEvent>(120)}, {TYPE_UINT8, 120});
}

TEST(EncodeDecode, int16)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(   1000LL)}, {TYPE_SINT16, 0xe8, 0x03});
    assert_encode_decode({std::make_shared<IntegerEvent>(  0x100LL)}, {TYPE_SINT16, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(  0x7ffLL)}, {TYPE_SINT16, 0xff, 0x07});
    assert_encode_decode({std::make_shared<IntegerEvent>(  0x8ffLL)}, {TYPE_SINT16, 0xff, 0x08});
    assert_encode_decode({std::make_shared<IntegerEvent>(  0x9ffLL)}, {TYPE_SINT16, 0xff, 0x09});
    assert_encode_decode({std::make_shared<IntegerEvent>(  0xfffLL)}, {TYPE_SINT16, 0xff, 0x0f});
    assert_encode_decode({std::make_shared<IntegerEvent>( 0x1000LL)}, {TYPE_SINT16, 0x00, 0x10});
    assert_encode_decode({std::make_shared<IntegerEvent>( 0x7fffLL)}, {TYPE_SINT16, 0xff, 0x7f});

    assert_encode_decode({std::make_shared<IntegerEvent>( 0x8000LL)}, {TYPE_UINT16, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>( 0xa011LL)}, {TYPE_UINT16, 0x11, 0xa0});
    assert_encode_decode({std::make_shared<IntegerEvent>( 0xffffLL)}, {TYPE_UINT16, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(  -0x81LL)}, {TYPE_SINT16, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(  -0xffLL)}, {TYPE_SINT16, 0x01, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>( -0x100LL)}, {TYPE_SINT16, 0x00, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>( -0x101LL)}, {TYPE_SINT16, 0xff, 0xfe});
    assert_encode_decode({std::make_shared<IntegerEvent>( -0x7ffLL)}, {TYPE_SINT16, 0x01, 0xf8});
    assert_encode_decode({std::make_shared<IntegerEvent>( -0x8ffLL)}, {TYPE_SINT16, 0x01, 0xf7});
    assert_encode_decode({std::make_shared<IntegerEvent>( -0x9ffLL)}, {TYPE_SINT16, 0x01, 0xf6});
    assert_encode_decode({std::make_shared<IntegerEvent>( -0xfffLL)}, {TYPE_SINT16, 0x01, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x1000LL)}, {TYPE_SINT16, 0x00, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000LL)}, {TYPE_SINT16, 0x00, 0x80});

    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_SINT16, 50, 0});
    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_UINT16, 50, 0});
    assert_decode({std::make_shared<IntegerEvent>(120)}, {TYPE_UINT16, 120, 0});
}

TEST(EncodeDecode, int24)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0x10000LL)},  {TYPE_SINT24, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x80000LL)},  {TYPE_SINT24, 0x00, 0x00, 0x08});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8ffffLL)},  {TYPE_SINT24, 0xff, 0xff, 0x08});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x9ffffLL)},  {TYPE_SINT24, 0xff, 0xff, 0x09});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xfffffLL)},  {TYPE_SINT24, 0xff, 0xff, 0x0f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x100000LL)}, {TYPE_SINT24, 0x00, 0x00, 0x10});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffffLL)}, {TYPE_SINT24, 0xff, 0xff, 0x7f});

    assert_encode_decode({std::make_shared<IntegerEvent>(0x800000LL)}, {TYPE_UINT24, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xa01234LL)}, {TYPE_UINT24, 0x34, 0x12, 0xa0});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xffffffLL)}, {TYPE_UINT24, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8001LL)},   {TYPE_SINT24, 0xff, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8fffLL)},   {TYPE_SINT24, 0x01, 0x70, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x9fffLL)},   {TYPE_SINT24, 0x01, 0x60, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xffffLL)},   {TYPE_SINT24, 0x01, 0x00, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x10000LL)},  {TYPE_SINT24, 0x00, 0x00, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000LL)},  {TYPE_SINT24, 0x00, 0x00, 0xf8});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8ffffLL)},  {TYPE_SINT24, 0x01, 0x00, 0xf7});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x9ffffLL)},  {TYPE_SINT24, 0x01, 0x00, 0xf6});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xfffffLL)},  {TYPE_SINT24, 0x01, 0x00, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x100000LL)}, {TYPE_SINT24, 0x00, 0x00, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x800000LL)}, {TYPE_SINT24, 0x00, 0x00, 0x80});

    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_SINT24, 50, 0, 0});
    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_UINT24, 50, 0, 0});
    assert_decode({std::make_shared<IntegerEvent>(120)}, {TYPE_UINT24, 120, 0, 0});
}

TEST(EncodeDecode, int32)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0x1000000LL)},  {TYPE_SINT32, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8000000LL)},  {TYPE_SINT32, 0x00, 0x00, 0x00, 0x08});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8ffffffLL)},  {TYPE_SINT32, 0xff, 0xff, 0xff, 0x08});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x9ffffffLL)},  {TYPE_SINT32, 0xff, 0xff, 0xff, 0x09});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xfffffffLL)},  {TYPE_SINT32, 0xff, 0xff, 0xff, 0x0f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x10000000LL)}, {TYPE_SINT32, 0x00, 0x00, 0x00, 0x10});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffffffLL)}, {TYPE_SINT32, 0xff, 0xff, 0xff, 0x7f});

    assert_encode_decode({std::make_shared<IntegerEvent>(0x80000000LL)}, {TYPE_UINT32, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8fffffffLL)}, {TYPE_UINT32, 0xff, 0xff, 0xff, 0x8f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x9fffffffLL)}, {TYPE_UINT32, 0xff, 0xff, 0xff, 0x9f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xffffffffLL)}, {TYPE_UINT32, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(-0x800001LL)},   {TYPE_SINT32, 0xff, 0xff, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8fffffLL)},   {TYPE_SINT32, 0x01, 0x00, 0x70, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x9fffffLL)},   {TYPE_SINT32, 0x01, 0x00, 0x60, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xffffffLL)},   {TYPE_SINT32, 0x01, 0x00, 0x00, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x1000000LL)},  {TYPE_SINT32, 0x00, 0x00, 0x00, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000000LL)},  {TYPE_SINT32, 0x00, 0x00, 0x00, 0xf8});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8ffffffLL)},  {TYPE_SINT32, 0x01, 0x00, 0x00, 0xf7});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x9ffffffLL)},  {TYPE_SINT32, 0x01, 0x00, 0x00, 0xf6});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xfffffffLL)},  {TYPE_SINT32, 0x01, 0x00, 0x00, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x10000000LL)}, {TYPE_SINT32, 0x00, 0x00, 0x00, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000000LL)}, {TYPE_SINT32, 0x00, 0x00, 0x00, 0x80});

    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_SINT32, 50, 0, 0, 0});
    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_UINT32, 50, 0, 0, 0});
    assert_decode({std::make_shared<IntegerEvent>(120)}, {TYPE_UINT32, 120, 0, 0, 0});
}

TEST(EncodeDecode, int40)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0x100000000LL)},  {TYPE_SINT40, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x800000000LL)},  {TYPE_SINT40, 0x00, 0x00, 0x00, 0x00, 0x08});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8ffffffffLL)},  {TYPE_SINT40, 0xff, 0xff, 0xff, 0xff, 0x08});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x9ffffffffLL)},  {TYPE_SINT40, 0xff, 0xff, 0xff, 0xff, 0x09});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xfffffffffLL)},  {TYPE_SINT40, 0xff, 0xff, 0xff, 0xff, 0x0f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x1000000000LL)}, {TYPE_SINT40, 0x00, 0x00, 0x00, 0x00, 0x10});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffffffffLL)}, {TYPE_SINT40, 0xff, 0xff, 0xff, 0xff, 0x7f});

    assert_encode_decode({std::make_shared<IntegerEvent>(0x8000000000LL)}, {TYPE_UINT40, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8fffffffffLL)}, {TYPE_UINT40, 0xff, 0xff, 0xff, 0xff, 0x8f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x9fffffffffLL)}, {TYPE_UINT40, 0xff, 0xff, 0xff, 0xff, 0x9f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xffffffffffLL)}, {TYPE_UINT40, 0xff, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000001LL)},   {TYPE_SINT40, 0xff, 0xff, 0xff, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8fffffffLL)},   {TYPE_SINT40, 0x01, 0x00, 0x00, 0x70, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x9fffffffLL)},   {TYPE_SINT40, 0x01, 0x00, 0x00, 0x60, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xffffffffLL)},   {TYPE_SINT40, 0x01, 0x00, 0x00, 0x00, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x100000000LL)},  {TYPE_SINT40, 0x00, 0x00, 0x00, 0x00, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x800000000LL)},  {TYPE_SINT40, 0x00, 0x00, 0x00, 0x00, 0xf8});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8ffffffffLL)},  {TYPE_SINT40, 0x01, 0x00, 0x00, 0x00, 0xf7});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x9ffffffffLL)},  {TYPE_SINT40, 0x01, 0x00, 0x00, 0x00, 0xf6});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xfffffffffLL)},  {TYPE_SINT40, 0x01, 0x00, 0x00, 0x00, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x1000000000LL)}, {TYPE_SINT40, 0x00, 0x00, 0x00, 0x00, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000000000LL)}, {TYPE_SINT40, 0x00, 0x00, 0x00, 0x00, 0x80});

    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_SINT40, 50, 0, 0, 0, 0});
    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_UINT40, 50, 0, 0, 0, 0});
    assert_decode({std::make_shared<IntegerEvent>(120)}, {TYPE_UINT40, 120, 0, 0, 0, 0});
}

TEST(EncodeDecode, int48)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0x10000000000LL)},  {TYPE_SINT48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x80000000000LL)},  {TYPE_SINT48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8ffffffffffLL)},  {TYPE_SINT48, 0xff, 0xff, 0xff, 0xff, 0xff, 0x08});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x9ffffffffffLL)},  {TYPE_SINT48, 0xff, 0xff, 0xff, 0xff, 0xff, 0x09});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xfffffffffffLL)},  {TYPE_SINT48, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x100000000000LL)}, {TYPE_SINT48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffffffffffLL)}, {TYPE_SINT48, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});

    assert_encode_decode({std::make_shared<IntegerEvent>(0x800000000000LL)}, {TYPE_UINT48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8fffffffffffLL)}, {TYPE_UINT48, 0xff, 0xff, 0xff, 0xff, 0xff, 0x8f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x9fffffffffffLL)}, {TYPE_UINT48, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xffffffffffffLL)}, {TYPE_UINT48, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000000001LL)},   {TYPE_SINT48, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8fffffffffLL)},   {TYPE_SINT48, 0x01, 0x00, 0x00, 0x00, 0x70, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x9fffffffffLL)},   {TYPE_SINT48, 0x01, 0x00, 0x00, 0x00, 0x60, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xffffffffffLL)},   {TYPE_SINT48, 0x01, 0x00, 0x00, 0x00, 0x00, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x10000000000LL)},  {TYPE_SINT48, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000000000LL)},  {TYPE_SINT48, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8ffffffffffLL)},  {TYPE_SINT48, 0x01, 0x00, 0x00, 0x00, 0x00, 0xf7});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x9ffffffffffLL)},  {TYPE_SINT48, 0x01, 0x00, 0x00, 0x00, 0x00, 0xf6});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xfffffffffffLL)},  {TYPE_SINT48, 0x01, 0x00, 0x00, 0x00, 0x00, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x100000000000LL)}, {TYPE_SINT48, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x800000000000LL)}, {TYPE_SINT48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});

    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_SINT48, 50, 0, 0, 0, 0, 0});
    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_UINT48, 50, 0, 0, 0, 0, 0});
    assert_decode({std::make_shared<IntegerEvent>(120)}, {TYPE_UINT48, 120, 0, 0, 0, 0, 0});
}

TEST(EncodeDecode, int56)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0x1000000000000LL)},  {TYPE_SINT56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8000000000000LL)},  {TYPE_SINT56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8ffffffffffffLL)},  {TYPE_SINT56, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x08});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x9ffffffffffffLL)},  {TYPE_SINT56, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x09});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xfffffffffffffLL)},  {TYPE_SINT56, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x10000000000000LL)}, {TYPE_SINT56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffffffffffffLL)}, {TYPE_SINT56, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});

    assert_encode_decode({std::make_shared<IntegerEvent>(0x80000000000000LL)}, {TYPE_UINT56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8fffffffffffffLL)}, {TYPE_UINT56, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x8f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x9fffffffffffffLL)}, {TYPE_UINT56, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xffffffffffffffLL)}, {TYPE_UINT56, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(-0x800000000001LL)},   {TYPE_SINT56, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8fffffffffffLL)},   {TYPE_SINT56, 0x01, 0x00, 0x00, 0x00, 0x00, 0x70, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x9fffffffffffLL)},   {TYPE_SINT56, 0x01, 0x00, 0x00, 0x00, 0x00, 0x60, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xffffffffffffLL)},   {TYPE_SINT56, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x1000000000000LL)},  {TYPE_SINT56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000000000000LL)},  {TYPE_SINT56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8ffffffffffffLL)},  {TYPE_SINT56, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x9ffffffffffffLL)},  {TYPE_SINT56, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf6});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xfffffffffffffLL)},  {TYPE_SINT56, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x10000000000000LL)}, {TYPE_SINT56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000000000000LL)}, {TYPE_SINT56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});

    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_SINT56, 50, 0, 0, 0, 0, 0, 0});
    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_UINT56, 50, 0, 0, 0, 0, 0, 0});
    assert_decode({std::make_shared<IntegerEvent>(120)}, {TYPE_UINT56, 120, 0, 0, 0, 0, 0, 0});
}

TEST(EncodeDecode, int64)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0x100000000000000LL)},  {TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x800000000000000LL)},  {TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8ffffffffffffffLL)},  {TYPE_SINT64, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x08});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x9ffffffffffffffLL)},  {TYPE_SINT64, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x09});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xfffffffffffffffLL)},  {TYPE_SINT64, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x1000000000000000LL)}, {TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffffffffffffffLL)}, {TYPE_SINT64, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});

    assert_encode_decode({std::make_shared<UIntegerEvent>(0x8000000000000000ULL)}, {TYPE_UINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x8000000000000001ULL)}, {TYPE_UINT64, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xffffffffffffffffULL)}, {TYPE_UINT64, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000000000001LL)},   {TYPE_SINT64, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8fffffffffffffLL)},   {TYPE_SINT64, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x9fffffffffffffLL)},   {TYPE_SINT64, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xffffffffffffffLL)},   {TYPE_SINT64, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x100000000000000LL)},  {TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x800000000000000LL)},  {TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8ffffffffffffffLL)},  {TYPE_SINT64, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x9ffffffffffffffLL)},  {TYPE_SINT64, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf6});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xfffffffffffffffLL)},  {TYPE_SINT64, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x1000000000000000LL)}, {TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000000000000000LL)}, {TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});

    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_SINT64, 50, 0, 0, 0, 0, 0, 0, 0});
    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_UINT64, 50, 0, 0, 0, 0, 0, 0, 0});
    assert_decode({std::make_shared<IntegerEvent>(120)}, {TYPE_UINT64, 120, 0, 0, 0, 0, 0, 0, 0});
}

TEST(EncodeDecode, BigNumber)
{
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0, 0))}, {TYPE_BIG_NUMBER, 0x00});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 0, 0))}, {TYPE_BIG_NUMBER, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 1, 0))}, {TYPE_BIG_NUMBER, 0x08, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 1, 0))}, {TYPE_BIG_NUMBER, 0x09, 0x01});

    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 1,  1))}, {TYPE_BIG_NUMBER, 0x0a, 0x01, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 1,  1))}, {TYPE_BIG_NUMBER, 0x0b, 0x01, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 1, -1))}, {TYPE_BIG_NUMBER, 0x0a, 0xff, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 1, -1))}, {TYPE_BIG_NUMBER, 0x0b, 0xff, 0x01});

    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x40, 0))}, {TYPE_BIG_NUMBER, 0x08, 0x40});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x80, 0))}, {TYPE_BIG_NUMBER, 0x08, 0x80});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x81, 0))}, {TYPE_BIG_NUMBER, 0x08, 0x81});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x90, 0))}, {TYPE_BIG_NUMBER, 0x08, 0x90});

    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x01, 0x40))}, {TYPE_BIG_NUMBER, 0x0a, 0x40, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x01, 0x80))}, {TYPE_BIG_NUMBER, 0x0c, 0x80, 0x00, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x01, 0x81))}, {TYPE_BIG_NUMBER, 0x0c, 0x81, 0x00, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x01, 0x90))}, {TYPE_BIG_NUMBER, 0x0c, 0x90, 0x00, 0x01});

    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x01, -0x40))}, {TYPE_BIG_NUMBER, 0x0a, 0xc0, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x01, -0x80))}, {TYPE_BIG_NUMBER, 0x0a, 0x80, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x01, -0x81))}, {TYPE_BIG_NUMBER, 0x0c, 0x7f, 0xff, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x01, -0x90))}, {TYPE_BIG_NUMBER, 0x0c, 0x70, 0xff, 0x01});

    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x123, 0))}, {TYPE_BIG_NUMBER, 0x10, 0x23, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 0x123, 0))}, {TYPE_BIG_NUMBER, 0x11, 0x23, 0x01});

    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x123,  0x456))}, {TYPE_BIG_NUMBER, 0x14, 0x56, 0x04, 0x23, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 0x123,  0x456))}, {TYPE_BIG_NUMBER, 0x15, 0x56, 0x04, 0x23, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0x123, -0x456))}, {TYPE_BIG_NUMBER, 0x14, 0xaa, 0xfb, 0x23, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 0x123, -0x456))}, {TYPE_BIG_NUMBER, 0x15, 0xaa, 0xfb, 0x23, 0x01});

    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 1,  0x7fffff))}, {TYPE_BIG_NUMBER, 0x0e, 0xff, 0xff, 0x7f, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 1,  0x7fffff))}, {TYPE_BIG_NUMBER, 0x0f, 0xff, 0xff, 0x7f, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 1, -0x800000))}, {TYPE_BIG_NUMBER, 0x0e, 0x00, 0x00, 0x80, 0x01});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 1, -0x800000))}, {TYPE_BIG_NUMBER, 0x0f, 0x00, 0x00, 0x80, 0x01});

    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0xffffffffffffffff,  0x7fffff))}, {TYPE_BIG_NUMBER, 0x46, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 0xffffffffffffffff,  0x7fffff))}, {TYPE_BIG_NUMBER, 0x47, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 0xffffffffffffffff, -0x800000))}, {TYPE_BIG_NUMBER, 0x46, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 0xffffffffffffffff, -0x800000))}, {TYPE_BIG_NUMBER, 0x47, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
}

TEST(EncodeDecode, short_string)
{
    assert_encode_decode({std::make_shared<StringEvent>("")}, {TYPE_STRING0});
    assert_encode_decode({std::make_shared<StringEvent>("a")}, {TYPE_STRING1, 'a'});
    assert_encode_decode({std::make_shared<StringEvent>("ab")}, {TYPE_STRING2, 'a', 'b'});
    assert_encode_decode({std::make_shared<StringEvent>("abc")}, {TYPE_STRING3, 'a', 'b', 'c'});
    assert_encode_decode({std::make_shared<StringEvent>("abcd")}, {TYPE_STRING4, 'a', 'b', 'c', 'd'});
    assert_encode_decode({std::make_shared<StringEvent>("abcde")}, {TYPE_STRING5, 'a', 'b', 'c', 'd', 'e'});
    assert_encode_decode({std::make_shared<StringEvent>("abcdef")}, {TYPE_STRING6, 'a', 'b', 'c', 'd', 'e', 'f'});
    assert_encode_decode({std::make_shared<StringEvent>("abcdefg")}, {TYPE_STRING7, 'a', 'b', 'c', 'd', 'e', 'f', 'g'});
    assert_encode_decode({std::make_shared<StringEvent>("abcdefgh")}, {TYPE_STRING8, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'});
    assert_encode_decode({std::make_shared<StringEvent>("abcdefghi")}, {TYPE_STRING9, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i'});
    assert_encode_decode({std::make_shared<StringEvent>("abcdefghij")}, {TYPE_STRING10, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'});
    assert_encode_decode({std::make_shared<StringEvent>("abcdefghijk")}, {TYPE_STRING11, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k'});
    assert_encode_decode({std::make_shared<StringEvent>("abcdefghijkl")}, {TYPE_STRING12, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l'});
    assert_encode_decode({std::make_shared<StringEvent>("abcdefghijklm")}, {TYPE_STRING13, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm'});
    assert_encode_decode({std::make_shared<StringEvent>("abcdefghijklmn")}, {TYPE_STRING14, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n'});
    assert_encode_decode({std::make_shared<StringEvent>("abcdefghijklmno")}, {TYPE_STRING15, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o'});
}

TEST(EncodeDecode, string)
{
    assert_encode_decode({std::make_shared<StringEvent>("1234567890123456789012345678901")},
        {
            TYPE_STRING, 0x7c,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31,
        });

    assert_encode_decode({std::make_shared<StringEvent>("12345678901234567890123456789012")},
        {
            TYPE_STRING, 0x80,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32,
        });

    assert_encode_decode({std::make_shared<StringEvent>("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890")},
        {
            TYPE_STRING, 0x11, 0x04,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
        });
}

TEST(EncodeDecode, array)
{
    // Empty array: TYPE_ARRAY + chunk header (0 elements, no continuation)
    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(0),
        std::make_shared<ContainerEndEvent>()
    },
    {TYPE_ARRAY, CHUNK(0)});

    // Array with 3 elements
    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<IntegerEvent>(1LL),
            std::make_shared<StringEvent>("x"),
            std::make_shared<NullEvent>(),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            SMALLINT(1),
            TYPE_STRING1, 'x',
            TYPE_NULL,
    });
}

TEST(EncodeDecode, object)
{
    // Empty object: TYPE_OBJECT + chunk header (0 pairs, no continuation)
    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(0),
        std::make_shared<ContainerEndEvent>()
    },
    {TYPE_OBJECT, CHUNK(0)});

    // Object with 3 pairs
    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(3),
            std::make_shared<StringEvent>("1"), std::make_shared<IntegerEvent>(1LL),
            std::make_shared<StringEvent>("2"), std::make_shared<StringEvent>("x"),
            std::make_shared<StringEvent>("3"), std::make_shared<NullEvent>(),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(3),
            TYPE_STRING1, '1', SMALLINT(1),
            TYPE_STRING1, '2', TYPE_STRING1, 'x',
            TYPE_STRING1, '3', TYPE_NULL,
    });
}


// ------------------------------------
// In-depth Tests
// ------------------------------------

TEST(Encoder, object_name)
{
    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(1),
            TYPE_STRING1, 'a',
            SMALLINT(1),
    });

    // Non-string is not allowed in the name field
    // Note: must use ObjectBeginEvent(1) to allocate space for a pair

    assert_encode_result(KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME,
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<IntegerEvent>(1LL),  // Should fail - expecting string key
    });

    assert_encode_result(KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME,
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<IntegerEvent>(1000LL),  // Should fail - expecting string key
    });

    assert_encode_result(KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME,
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<IntegerEvent>(0x1000000000000000LL),  // Should fail - expecting string key
    });

    assert_encode_result(KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME,
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<BooleanEvent>(true),  // Should fail - expecting string key
    });

    assert_encode_result(KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME,
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<FloatEvent>(1.234),  // Should fail - expecting string key
    });

    assert_encode_result(KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME,
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<NullEvent>(),  // Should fail - expecting string key
    });

    assert_encode_result(KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME,
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<ObjectBeginEvent>(0),  // Should fail - expecting string key
    });

    assert_encode_result(KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME,
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<ArrayBeginEvent>(0),  // Should fail - expecting string key
    });
}

TEST(Encoder, object_value)
{
    // With chunked containers, objects can't be manually closed.
    // If an object is incomplete (expecting a value), endEncode will fail
    // with CONTAINERS_ARE_STILL_OPEN.
    assert_encode_result(KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN,
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<StringEvent>("a"),
        // No value provided - object can't auto-close
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_result(KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN,
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1LL),
            std::make_shared<StringEvent>("z"),
        // Second pair incomplete - object can't auto-close
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(2),
            TYPE_STRING1, 'a',
            SMALLINT(1),
            TYPE_STRING1, 'z',
            SMALLINT(1),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(-1LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(2),
            TYPE_STRING1, 'a',
            SMALLINT(-1),
            TYPE_STRING1, 'z',
            SMALLINT(1),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1000LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(2),
            TYPE_STRING1, 'a',
            TYPE_SINT16, 0xe8, 0x03,
            TYPE_STRING1, 'z',
            SMALLINT(1),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(0x1000000000000000LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(2),
            TYPE_STRING1, 'a',
            TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
            TYPE_STRING1, 'z',
            SMALLINT(1),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(-0x1000000000000000LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(2),
            TYPE_STRING1, 'a',
            TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0,
            TYPE_STRING1, 'z',
            SMALLINT(1),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("a"),
            std::make_shared<FloatEvent>(1.25),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(2),
            TYPE_STRING1, 'a',
            TYPE_FLOAT16, 0xa0, 0x3f,
            TYPE_STRING1, 'z',
            SMALLINT(1),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("a"),
            std::make_shared<FloatEvent>(-5.923441e-50),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(2),
            TYPE_STRING1, 'a',
            TYPE_FLOAT64, 0x35, 0x3c, 0xce, 0x81, 0x87, 0x29, 0xb6, 0xb5,
            TYPE_STRING1, 'z',
            SMALLINT(1),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("a"),
            std::make_shared<StringEvent>("b"),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(2),
            TYPE_STRING1, 'a',
            TYPE_STRING1, 'b',
            TYPE_STRING1, 'z',
            SMALLINT(1),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("a"),
            std::make_shared<BooleanEvent>(false),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(2),
            TYPE_STRING1, 'a',
            TYPE_FALSE,
            TYPE_STRING1, 'z',
            SMALLINT(1),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("a"),
            std::make_shared<NullEvent>(),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(2),
            TYPE_STRING1, 'a',
            TYPE_NULL,
            TYPE_STRING1, 'z',
            SMALLINT(1),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ObjectBeginEvent>(0),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(2),
            TYPE_STRING1, 'a',
            TYPE_OBJECT, CHUNK(0),
            TYPE_STRING1, 'z',
            SMALLINT(1),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ArrayBeginEvent>(0),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(2),
            TYPE_STRING1, 'a',
            TYPE_ARRAY, CHUNK(0),
            TYPE_STRING1, 'z',
            SMALLINT(1),
    });
}

TEST(Encoder, array_value)
{
    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1LL),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            TYPE_STRING1, 'a',
            SMALLINT(1),
            TYPE_STRING1, 'z',
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(-1LL),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            TYPE_STRING1, 'a',
            SMALLINT(-1),
            TYPE_STRING1, 'z',
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1000LL),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            TYPE_STRING1, 'a',
            TYPE_SINT16, 0xe8, 0x03,
            TYPE_STRING1, 'z',
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(0x1000000000000000LL),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            TYPE_STRING1, 'a',
            TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
            TYPE_STRING1, 'z',
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(-0x1000000000000000LL),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            TYPE_STRING1, 'a',
            TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0,
            TYPE_STRING1, 'z',
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<StringEvent>("a"),
            std::make_shared<FloatEvent>(1.25),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            TYPE_STRING1, 'a',
            TYPE_FLOAT16, 0xa0, 0x3f,
            TYPE_STRING1, 'z',
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<StringEvent>("a"),
            std::make_shared<FloatEvent>(-5.923441e-50),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            TYPE_STRING1, 'a',
            TYPE_FLOAT64, 0x35, 0x3c, 0xce, 0x81, 0x87, 0x29, 0xb6, 0xb5,
            TYPE_STRING1, 'z',
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<StringEvent>("a"),
            std::make_shared<StringEvent>("b"),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            TYPE_STRING1, 'a',
            TYPE_STRING1, 'b',
            TYPE_STRING1, 'z',
    });

    assert_encode(
    {
        std::make_shared<ArrayBeginEvent>(4),
            std::make_shared<StringEvent>("a"),
            std::make_shared<StringChunkEvent>("b", CHUNK_HAS_NEXT),
            std::make_shared<StringChunkEvent>("cdefg", CHUNK_HAS_NEXT),
            std::make_shared<StringChunkEvent>("h", CHUNK_HAS_NEXT),
            std::make_shared<StringChunkEvent>("i", CHUNK_LAST),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(4),
            TYPE_STRING1, 'a',
            TYPE_STRING, 0x06, 'b', 0x16, 'c', 'd', 'e', 'f', 'g', 0x06, 'h', 0x04, 'i',
            TYPE_STRING1, 'z',
            SMALLINT(1),
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<StringEvent>("a"),
            std::make_shared<BooleanEvent>(false),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            TYPE_STRING1, 'a',
            TYPE_FALSE,
            TYPE_STRING1, 'z',
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<StringEvent>("a"),
            std::make_shared<NullEvent>(),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            TYPE_STRING1, 'a',
            TYPE_NULL,
            TYPE_STRING1, 'z',
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ObjectBeginEvent>(0),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            TYPE_STRING1, 'a',
            TYPE_OBJECT, CHUNK(0),
            TYPE_STRING1, 'z',
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ArrayBeginEvent>(0),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            TYPE_STRING1, 'a',
            TYPE_ARRAY, CHUNK(0),
            TYPE_STRING1, 'z',
    });
}


// ------------------------------------
// Failure Modes
// ------------------------------------

TEST(Encoder, failed_to_add)
{
    KSBONJSONEncodeContext eContext;
    memset(&eContext, 0, sizeof(eContext));
    EncoderContext eCtx(10000);
    ksbonjson_beginEncode(&eContext, addEncodedDataFailCallback, &eCtx);
    ASSERT_NE(KSBONJSON_ENCODE_OK, (*std::make_shared<IntegerEvent>(1LL))(&eContext));
}

TEST(Encoder, fail_string_chunking)
{
    assert_encode_result(KSBONJSON_ENCODE_CHUNKING_STRING,
    {
        std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
    });

    assert_encode_result(KSBONJSON_ENCODE_CHUNKING_STRING,
    {
        std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
        std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
        std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
        std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
        std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
    });

    assert_encode_result(KSBONJSON_ENCODE_CHUNKING_STRING,
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_result(KSBONJSON_ENCODE_CHUNKING_STRING,
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_result(KSBONJSON_ENCODE_CHUNKING_STRING,
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
        std::make_shared<ContainerEndEvent>(),
    });
}

TEST(Encoder, fail_containers)
{
    // With chunked containers, containers auto-close when their element count is reached.
    // Empty containers (0 elements) auto-close immediately.
    // To test CONTAINERS_ARE_STILL_OPEN, we need containers with non-zero counts
    // that don't have all their elements added.

    // Object expecting 1 pair but not provided
    assert_encode_result(KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN,
    {
        std::make_shared<ObjectBeginEvent>(1),
    });

    // Outer object expecting 1 pair, but nested empty object doesn't count as value
    // (value is pending)
    assert_encode_result(KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN,
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ObjectBeginEvent>(1),  // This is the value, but it needs 1 pair
    });

    // Similar with array as value
    assert_encode_result(KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN,
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ArrayBeginEvent>(1),  // Array needs 1 element
    });

    // Array expecting 1 element but not provided
    assert_encode_result(KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN,
    {
        std::make_shared<ArrayBeginEvent>(1),
    });

    // Nested array incomplete
    assert_encode_result(KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN,
    {
        std::make_shared<ArrayBeginEvent>(1),
            std::make_shared<ArrayBeginEvent>(1),  // Inner array needs 1 element
    });

    // Array with nested object incomplete
    assert_encode_result(KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN,
    {
        std::make_shared<ArrayBeginEvent>(1),
            std::make_shared<ObjectBeginEvent>(1),  // Object needs 1 pair
    });

    // Object inside array with incomplete key-value pair
    assert_encode_result(KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN,
    {
        std::make_shared<ArrayBeginEvent>(1),
            std::make_shared<ObjectBeginEvent>(1),
                std::make_shared<StringEvent>("a"),  // Key provided, no value
    });

    // With chunked containers, ContainerEndEvent is a no-op.
    // Empty containers auto-close, so extra ContainerEndEvents don't cause errors.
    // Use assert_encode since extra no-op events won't round-trip.
    assert_encode(
    {
        std::make_shared<ObjectBeginEvent>(0),
        std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),  // Extra ContainerEndEvents are no-ops
    },
    {
        TYPE_OBJECT, CHUNK(0),
    });

    assert_encode(
    {
        std::make_shared<ArrayBeginEvent>(0),
        std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),  // Extra ContainerEndEvents are no-ops
    },
    {
        TYPE_ARRAY, CHUNK(0),
    });

    // Nested containers that complete correctly
    assert_encode(
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ObjectBeginEvent>(0),  // Empty inner object completes as value
            std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),  // Extra ContainerEndEvents are no-ops
    },
    {
        TYPE_OBJECT, CHUNK(1),
            TYPE_STRING1, 'a',
            TYPE_OBJECT, CHUNK(0),
    });

    assert_encode(
    {
        std::make_shared<ObjectBeginEvent>(1),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ArrayBeginEvent>(0),  // Empty inner array completes as value
            std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),  // Extra ContainerEndEvents are no-ops
    },
    {
        TYPE_OBJECT, CHUNK(1),
            TYPE_STRING1, 'a',
            TYPE_ARRAY, CHUNK(0),
    });

    assert_encode(
    {
        std::make_shared<ArrayBeginEvent>(1),
            std::make_shared<ObjectBeginEvent>(0),  // Empty inner object completes as element
            std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),  // Extra ContainerEndEvents are no-ops
    },
    {
        TYPE_ARRAY, CHUNK(1),
            TYPE_OBJECT, CHUNK(0),
    });

    assert_encode(
    {
        std::make_shared<ArrayBeginEvent>(1),
            std::make_shared<ArrayBeginEvent>(0),  // Empty inner array completes as element
            std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),  // Extra ContainerEndEvents are no-ops
    },
    {
        TYPE_ARRAY, CHUNK(1),
            TYPE_ARRAY, CHUNK(0),
    });
}

TEST(Decoder, unbalanced_containers)
{
    // New spec: containers use chunk headers instead of TYPE_END
    // These test for incomplete containers where document ends before all elements are provided

    // Missing chunk header
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_OBJECT});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_ARRAY});

    // Container expects more elements than provided
    assert_decode_result(KSBONJSON_DECODE_UNCLOSED_CONTAINERS, {TYPE_OBJECT, CHUNK(1), TYPE_STRING0});  // expects value
    assert_decode_result(KSBONJSON_DECODE_UNCLOSED_CONTAINERS, {TYPE_ARRAY, CHUNK(1)});  // expects 1 element

    // Nested containers with incomplete inner container
    assert_decode_result(KSBONJSON_DECODE_UNCLOSED_CONTAINERS, {TYPE_OBJECT, CHUNK(1), TYPE_STRING0, TYPE_OBJECT, CHUNK(1)});
    assert_decode_result(KSBONJSON_DECODE_UNCLOSED_CONTAINERS, {TYPE_OBJECT, CHUNK(1), TYPE_STRING0, TYPE_ARRAY, CHUNK(1)});
    assert_decode_result(KSBONJSON_DECODE_UNCLOSED_CONTAINERS, {TYPE_ARRAY, CHUNK(1), TYPE_ARRAY, CHUNK(1)});
    assert_decode_result(KSBONJSON_DECODE_UNCLOSED_CONTAINERS, {TYPE_ARRAY, CHUNK(1), TYPE_OBJECT, CHUNK(1)});
}

TEST(Decoder, fail_string)
{
    assert_decode_result(KSBONJSON_DECODE_NUL_CHARACTER, {TYPE_STRING1, 0x00});
    assert_decode_result(KSBONJSON_DECODE_NUL_CHARACTER, {TYPE_STRING2, 'a', 0x00});
    assert_decode_result(KSBONJSON_DECODE_NUL_CHARACTER, {TYPE_STRING2, 0x00, 'a'});
    assert_decode_result(KSBONJSON_DECODE_NUL_CHARACTER, {TYPE_STRING, 0x40, 't', 'h', 'i', 's', ' ', 'i', 's', ' ', 'a', ' ', 's', 't', 'r', 0x00, 'n', 'g'});
}

TEST(Decoder, fail_big_number)
{
    // NaN, Infinity
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {TYPE_BIG_NUMBER, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {TYPE_BIG_NUMBER, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {TYPE_BIG_NUMBER, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {TYPE_BIG_NUMBER, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {TYPE_BIG_NUMBER, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {TYPE_BIG_NUMBER, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});

    // Significand too big (max 8 bytes)
    assert_decode_result(KSBONJSON_DECODE_VALUE_OUT_OF_RANGE, {TYPE_BIG_NUMBER, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});

    // Exponent too big (min -0x800000, max 0x7fffff)
    assert_encode_result(KSBONJSON_ENCODE_INVALID_DATA, {std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 1,  0x800000))});
    assert_encode_result(KSBONJSON_ENCODE_INVALID_DATA, {std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 1,  0x800000))});
    assert_encode_result(KSBONJSON_ENCODE_INVALID_DATA, {std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber( 1, 1, -0x800001))});
    assert_encode_result(KSBONJSON_ENCODE_INVALID_DATA, {std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 1, -0x800001))});
}

TEST(Decoder, fail_float)
{
    assert_encode_result(KSBONJSON_ENCODE_INVALID_DATA, {std::make_shared<FloatEvent>(NAN)});
    assert_encode_result(KSBONJSON_ENCODE_INVALID_DATA, {std::make_shared<FloatEvent>(INFINITY)});
    assert_encode_result(KSBONJSON_ENCODE_INVALID_DATA, {std::make_shared<FloatEvent>(-INFINITY)});
}

TEST(Decoder, fail_truncated)
{
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_UINT8});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_UINT16, 0x02});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_UINT24, 0x02, 0x02});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_UINT32, 0x02, 0x02, 0x02});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_UINT40, 0x02, 0x02, 0x02, 0x02});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_UINT48, 0x02, 0x02, 0x02, 0x02, 0x02});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_UINT56, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_UINT64, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02});

    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_SINT8});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_SINT16, 0x02});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_SINT24, 0x02, 0x02});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_SINT32, 0x02, 0x02, 0x02});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_SINT40, 0x02, 0x02, 0x02, 0x02});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_SINT48, 0x02, 0x02, 0x02, 0x02, 0x02});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_SINT56, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_SINT64, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02});

    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_FLOAT16, 0x00});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_FLOAT32, 0x00, 0x00, 0x00});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_FLOAT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});

    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_BIG_NUMBER, 0x08});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_BIG_NUMBER, 0x10, 0x00});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_BIG_NUMBER, 0x18, 0x00, 0x00});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_BIG_NUMBER, 0x0c, 0x00});

    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING1});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING2, 'a'});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING3, 'a', 'a'});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING4, 'a', 'a', 'a'});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING5, 'a', 'a', 'a', 'a'});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING6, 'a', 'a', 'a', 'a', 'a'});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING7, 'a', 'a', 'a', 'a', 'a', 'a'});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING8, 'a', 'a', 'a', 'a', 'a', 'a', 'a'});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING9, 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a'});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING10, 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a'});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING11, 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a'});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING12, 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a'});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING13, 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a'});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING14, 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a'});
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRING15, 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a'});
}

TEST(Decoder, fail_invalid_type_code)
{
    // New spec reserved codes: 0xc9-0xcf and 0xfa-0xff
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xc9});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xca});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xcb});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xcc});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xcd});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xce});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xcf});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xfa});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xfb});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xfc});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xfd});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xfe});
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xff});
}

// ------------------------------------
// Example Tests
// ------------------------------------

TEST(Examples, specification)
{
    // Short String (0xe0-0xef)
    assert_encode_decode({std::make_shared<StringEvent>("")},  {TYPE_STRING0});  // 0xe0
    assert_encode_decode({std::make_shared<StringEvent>("A")},  {TYPE_STRING1, 0x41});  // 0xe1
    assert_encode_decode({std::make_shared<StringEvent>("おはよう")},  {TYPE_STRING12, 0xe3, 0x81, 0x8a, 0xe3, 0x81, 0xaf, 0xe3, 0x82, 0x88, 0xe3, 0x81, 0x86});
    assert_encode_decode({std::make_shared<StringEvent>("15 byte string!")},  {TYPE_STRING15, 0x31, 0x35, 0x20, 0x62, 0x79, 0x74, 0x65, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x21});

    // Long String (0xf0 + chunk header)
    assert_decode({std::make_shared<StringEvent>("")}, {TYPE_STRING, 0x00});
    assert_decode({std::make_shared<StringEvent>("a string")}, {TYPE_STRING, 0x20, 0x61, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67});
    assert_decode(
        {
            std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
            std::make_shared<StringChunkEvent>(" str", CHUNK_HAS_NEXT),
            std::make_shared<StringChunkEvent>("ing", CHUNK_LAST)
        },
        {TYPE_STRING, 0x06, 0x61, 0x12, 0x20, 0x73, 0x74, 0x72, 0x0c, 0x69, 0x6e, 0x67});
    assert_encode_decode(
        {std::make_shared<StringEvent>("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ")},
        {TYPE_STRING, 0x01, 0x02,
        0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
        0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
        0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
        0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
    });

    // Small Integer (value + 100 = type_code)
    assert_encode_decode({std::make_shared<IntegerEvent>( 100)},  {SMALLINT(100)});   // 0xc8
    assert_encode_decode({std::make_shared<IntegerEvent>(   5)},  {SMALLINT(5)});     // 0x69
    assert_encode_decode({std::make_shared<IntegerEvent>(   0)},  {SMALLINT(0)});     // 0x64
    assert_encode_decode({std::make_shared<IntegerEvent>( -60)},  {SMALLINT(-60)});   // 0x28
    assert_encode_decode({std::make_shared<IntegerEvent>(-100)},  {SMALLINT(-100)});  // 0x00

    // Integer (0xd0-0xdf)
    assert_encode_decode({std::make_shared<IntegerEvent>(180)}, {TYPE_UINT8, 0xb4});
    assert_encode_decode({std::make_shared<IntegerEvent>(-1000)}, {TYPE_SINT16, 0x18, 0xfc});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8000)}, {TYPE_UINT16, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x123456789abcLL)}, {TYPE_SINT48, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000000000000000LL)}, {TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xded0d0d0dedadada)}, {TYPE_UINT64, 0xda, 0xda, 0xda, 0xde, 0xd0, 0xd0, 0xd0, 0xde});

    // 16 Bit Float (0xf2)
    assert_encode_decode({std::make_shared<FloatEvent>(1.125)},  {TYPE_FLOAT16, 0x90, 0x3f});

    // 32 Bit Float (0xf3)
    assert_encode_decode({std::make_shared<FloatEvent>(0x1.3f7p5)},  {TYPE_FLOAT32, 0x00, 0xb8, 0x1f, 0x42});

    // 64 Bit Float (0xf4)
    assert_encode_decode({std::make_shared<FloatEvent>(1.234)},  {TYPE_FLOAT64, 0x58, 0x39, 0xb4, 0xc8, 0x76, 0xbe, 0xf3, 0x3f});

    // Big Number (0xf1)
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(1, 15, -1))},  {TYPE_BIG_NUMBER, 0x0a, 0xff, 0x0f});
    assert_encode_decode({std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 0, 0))},  {TYPE_BIG_NUMBER, 0x01});

    // Array (0xf8 + chunk header)
    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(3),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1),
            std::make_shared<NullEvent>(),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY, CHUNK(3),
            TYPE_STRING1, 0x61,
            SMALLINT(1),
            TYPE_NULL,
    });

    // Object (0xf9 + chunk header)
    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(2),
            std::make_shared<StringEvent>("b"),
            std::make_shared<IntegerEvent>(0),
            std::make_shared<StringEvent>("test"),
            std::make_shared<StringEvent>("x"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(2),
            TYPE_STRING1, 0x62,
            SMALLINT(0),
            TYPE_STRING4, 0x74, 0x65, 0x73, 0x74,
            TYPE_STRING1, 0x78,
    });

    // Boolean (0xf6, 0xf7)
    assert_encode_decode({std::make_shared<BooleanEvent>(false)}, {TYPE_FALSE});
    assert_encode_decode({std::make_shared<BooleanEvent>(true)}, {TYPE_TRUE});

    // Null (0xf5)
    assert_encode_decode({std::make_shared<NullEvent>()}, {TYPE_NULL});

    // Full Example
    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(5),
            std::make_shared<StringEvent>("number"),
            std::make_shared<IntegerEvent>(50LL),
            std::make_shared<StringEvent>("null"),
            std::make_shared<NullEvent>(),
            std::make_shared<StringEvent>("boolean"),
            std::make_shared<BooleanEvent>(true),
            std::make_shared<StringEvent>("array"),
            std::make_shared<ArrayBeginEvent>(3),
                std::make_shared<StringEvent>("x"),
                std::make_shared<IntegerEvent>(1000LL),
                std::make_shared<FloatEvent>(-1.25),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("object"),
            std::make_shared<ObjectBeginEvent>(2),
                std::make_shared<StringEvent>("negative number"),
                std::make_shared<IntegerEvent>(-100LL),
                std::make_shared<StringEvent>("long string"),
                std::make_shared<StringEvent>("1234567890123456789012345678901234567890"),
            std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT, CHUNK(5),
            TYPE_STRING6, 0x6e, 0x75, 0x6d, 0x62, 0x65, 0x72,  // "number"
            SMALLINT(50),                                       // 50
            TYPE_STRING4, 0x6e, 0x75, 0x6c, 0x6c,              // "null"
            TYPE_NULL,                                          // null
            TYPE_STRING7, 0x62, 0x6f, 0x6f, 0x6c, 0x65, 0x61, 0x6e,  // "boolean"
            TYPE_TRUE,                                          // true
            TYPE_STRING5, 0x61, 0x72, 0x72, 0x61, 0x79,        // "array"
            TYPE_ARRAY, CHUNK(3),                               // array with 3 elements
                TYPE_STRING1, 0x78,                             // "x"
                TYPE_SINT16, 0xe8, 0x03,                        // 1000
                TYPE_FLOAT16, 0xa0, 0xbf,                       // -1.25
            TYPE_STRING6, 0x6f, 0x62, 0x6a, 0x65, 0x63, 0x74,  // "object"
            TYPE_OBJECT, CHUNK(2),                              // object with 2 pairs
                TYPE_STRING15, 0x6e, 0x65, 0x67, 0x61, 0x74, 0x69, 0x76, 0x65, 0x20, 0x6e, 0x75, 0x6d, 0x62, 0x65, 0x72,  // "negative number"
                SMALLINT(-100),                                 // -100
                TYPE_STRING11, 0x6c, 0x6f, 0x6e, 0x67, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,  // "long string"
                TYPE_STRING, 0xa0,                              // long string (40 bytes)
                      0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                      0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                      0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                      0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
    });
}
