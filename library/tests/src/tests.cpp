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

    ksbonjson_decodeStatus onValue(int64_t value)
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

    ksbonjson_decodeStatus onBeginObject() override
    {
        addEvent(std::make_shared<ObjectBeginEvent>());
        return KSBONJSON_DECODE_OK;
    }

    ksbonjson_decodeStatus onBeginArray() override
    {
        addEvent(std::make_shared<ArrayBeginEvent>());
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

void assert_decode(std::vector<uint8_t> document, std::vector<std::shared_ptr<Event>> expected_events)
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

void assert_encode_failure(std::vector<std::shared_ptr<Event>> events)
{
    if(REPORT_ENCODING)
    {
        printf("\n[assert_encode_failure]\n");
    }

    KSBONJSONEncodeContext eContext;
    memset(&eContext, 0, sizeof(eContext));
    EncoderContext eCtx(10000);
    ksbonjson_beginEncode(&eContext, addEncodedDataCallback, &eCtx);
    for (const std::shared_ptr<Event>& event: events)
    {
        if((*event)(&eContext) != KSBONJSON_ENCODE_OK)
        {
            SUCCEED();
            return;
        }
    }
    if(ksbonjson_endEncode(&eContext) != KSBONJSON_ENCODE_OK)
    {
        SUCCEED();
        return;
    }
    FAIL();
}

void assert_decode_failure(std::vector<uint8_t> document)
{
    if(REPORT_DECODING)
    {
        printf("\n[assert_decode_failure]\n");
    }

    Decoder decoder;
    size_t decodedOffset = 0;
    ASSERT_NE(KSBONJSON_ENCODE_OK, decoder.decode(document, &decodedOffset));
}

// ============================================================================
// Tests
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
    INTSMALL_NEGATIVE_EDGE = (unsigned char)-106,
    INTSMALL_POSITIVE_EDGE = 106,
};

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
}

TEST(EncodeDecode, float64)
{
    assert_encode_decode({std::make_shared<FloatEvent>(1.234)}, {TYPE_FLOAT64, 0x58, 0x39, 0xb4, 0xc8, 0x76, 0xbe, 0xf3, 0x3f});
}

TEST(EncodeDecode, smallint)
{
    assert_encode_decode({std::make_shared<IntegerEvent>( 106)}, {(uint8_t) 106});
    assert_encode_decode({std::make_shared<IntegerEvent>( 100)}, {(uint8_t) 100});
    assert_encode_decode({std::make_shared<IntegerEvent>(  10)}, {(uint8_t)  10});
    assert_encode_decode({std::make_shared<IntegerEvent>(   0)}, {(uint8_t)   0});
    assert_encode_decode({std::make_shared<IntegerEvent>(   1)}, {(uint8_t)   1});
    assert_encode_decode({std::make_shared<IntegerEvent>(  -1)}, {(uint8_t)  -1});
    assert_encode_decode({std::make_shared<IntegerEvent>( -60)}, {(uint8_t) -60});
    assert_encode_decode({std::make_shared<IntegerEvent>(-100)}, {(uint8_t)-100});
    assert_encode_decode({std::make_shared<IntegerEvent>(-106)}, {(uint8_t)-106});
}

TEST(EncodeDecode, int8)
{
    assert_encode_decode({std::make_shared<IntegerEvent>( 127)}, {TYPE_SINT8, (uint8_t)( 127)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 126)}, {TYPE_SINT8, (uint8_t)( 126)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 122)}, {TYPE_SINT8, (uint8_t)( 122)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 107)}, {TYPE_SINT8, (uint8_t)( 107)});

    assert_encode_decode({std::make_shared<IntegerEvent>( 128)}, {TYPE_UINT8, (uint8_t)( 128)}); 
    assert_encode_decode({std::make_shared<IntegerEvent>( 200)}, {TYPE_UINT8, (uint8_t)( 200)}); 
    assert_encode_decode({std::make_shared<IntegerEvent>( 255)}, {TYPE_UINT8, (uint8_t)( 255)}); 

    assert_encode_decode({std::make_shared<IntegerEvent>(-107)}, {TYPE_SINT8, (uint8_t)(-107)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-121)}, {TYPE_SINT8, (uint8_t)(-121)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-123)}, {TYPE_SINT8, (uint8_t)(-123)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-127)}, {TYPE_SINT8, (uint8_t)(-127)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-128)}, {TYPE_SINT8, (uint8_t)(-128)});

    assert_decode({TYPE_SINT8, 50}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT8, 50}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT8, 120}, {std::make_shared<IntegerEvent>(120)});
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

    assert_decode({TYPE_SINT16, 50, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT16, 50, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT16, 120, 0}, {std::make_shared<IntegerEvent>(120)});
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

    assert_decode({TYPE_SINT24, 50, 0, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT24, 50, 0, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT24, 120, 0, 0}, {std::make_shared<IntegerEvent>(120)});
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

    assert_decode({TYPE_SINT32, 50, 0, 0, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT32, 50, 0, 0, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT32, 120, 0, 0, 0}, {std::make_shared<IntegerEvent>(120)});
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

    assert_decode({TYPE_SINT40, 50, 0, 0, 0, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT40, 50, 0, 0, 0, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT40, 120, 0, 0, 0, 0}, {std::make_shared<IntegerEvent>(120)});
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

    assert_decode({TYPE_SINT48, 50, 0, 0, 0, 0, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT48, 50, 0, 0, 0, 0, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT48, 120, 0, 0, 0, 0, 0}, {std::make_shared<IntegerEvent>(120)});
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

    assert_decode({TYPE_SINT56, 50, 0, 0, 0, 0, 0, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT56, 50, 0, 0, 0, 0, 0, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT56, 120, 0, 0, 0, 0, 0, 0}, {std::make_shared<IntegerEvent>(120)});
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

    assert_decode({TYPE_SINT64, 50, 0, 0, 0, 0, 0, 0, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT64, 50, 0, 0, 0, 0, 0, 0, 0}, {std::make_shared<IntegerEvent>(50)});
    assert_decode({TYPE_UINT64, 120, 0, 0, 0, 0, 0, 0, 0}, {std::make_shared<IntegerEvent>(120)});
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
            TYPE_STRING,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31,
            STRING_TERMINATOR,
        });

    assert_encode_decode({std::make_shared<StringEvent>("12345678901234567890123456789012")},
        {
            TYPE_STRING,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32,
            STRING_TERMINATOR,
        });

    assert_encode_decode({std::make_shared<StringEvent>("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890")},
        {
            TYPE_STRING,
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
            STRING_TERMINATOR,
        });
}

TEST(EncodeDecode, array)
{
    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
        std::make_shared<ContainerEndEvent>()
    },
    {TYPE_ARRAY, TYPE_END});

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<IntegerEvent>(1LL),
            std::make_shared<StringEvent>("x"),
            std::make_shared<NullEvent>(),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY,
            1,
            TYPE_STRING1, 'x',
            TYPE_NULL,
        TYPE_END
    });
}

TEST(EncodeDecode, object)
{
    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
        std::make_shared<ContainerEndEvent>()
    },
    {TYPE_OBJECT, TYPE_END});

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("1"), std::make_shared<IntegerEvent>(1LL),
            std::make_shared<StringEvent>("2"), std::make_shared<StringEvent>("x"),
            std::make_shared<StringEvent>("3"), std::make_shared<NullEvent>(),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, '1', 1,
            TYPE_STRING1, '2', TYPE_STRING1, 'x',
            TYPE_STRING1, '3', TYPE_NULL,
        TYPE_END
    });
}


// ------------------------------------
// In-depth Tests
// ------------------------------------

TEST(Encoder, object_name)
{
    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, 'a',
            1,
        TYPE_END,
    });

    // Non-string is not allowed in the name field

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<IntegerEvent>(1LL),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<IntegerEvent>(1000LL),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<IntegerEvent>(0x1000000000000000LL),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<BooleanEvent>(true),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<FloatEvent>(1.234),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<NullEvent>(),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    });
}

TEST(Encoder, object_value)
{
    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1LL),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, 'a',
            1,
            TYPE_STRING1, 'z',
            1,
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(-1LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, 'a',
            (uint8_t)-1,
            TYPE_STRING1, 'z',
            1,
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1000LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, 'a',
            TYPE_SINT16, 0xe8, 0x03,
            TYPE_STRING1, 'z',
            1,
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(0x1000000000000000LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, 'a',
            TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
            TYPE_STRING1, 'z',
            1,
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(-0x1000000000000000LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, 'a',
            TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0,
            TYPE_STRING1, 'z',
            1,
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<FloatEvent>(1.25),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, 'a',
            TYPE_FLOAT16, 0xa0, 0x3f,
            TYPE_STRING1, 'z',
            1,
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<FloatEvent>(-5.923441e-50),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, 'a',
            TYPE_FLOAT64, 0x35, 0x3c, 0xce, 0x81, 0x87, 0x29, 0xb6, 0xb5,
            TYPE_STRING1, 'z',
            1,
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<StringEvent>("b"),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, 'a',
            TYPE_STRING1, 'b',
            TYPE_STRING1, 'z',
            1,
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<BooleanEvent>(false),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, 'a',
            TYPE_FALSE,
            TYPE_STRING1, 'z',
            1,
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<NullEvent>(),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, 'a',
            TYPE_NULL,
            TYPE_STRING1, 'z',
            1,
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, 'a',
            TYPE_OBJECT,
            TYPE_END,
            TYPE_STRING1, 'z',
            1,
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_OBJECT,
            TYPE_STRING1, 'a',
            TYPE_ARRAY,
            TYPE_END,
            TYPE_STRING1, 'z',
            1,
        TYPE_END,
    });
}

TEST(Encoder, array_value)
{
    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1LL),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY,
            TYPE_STRING1, 'a',
            1,
            TYPE_STRING1, 'z',
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(-1LL),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY,
            TYPE_STRING1, 'a',
            (uint8_t)-1,
            TYPE_STRING1, 'z',
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1000LL),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY,
            TYPE_STRING1, 'a',
            TYPE_SINT16, 0xe8, 0x03,
            TYPE_STRING1, 'z',
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(0x1000000000000000LL),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY,
            TYPE_STRING1, 'a',
            TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
            TYPE_STRING1, 'z',
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(-0x1000000000000000LL),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY,
            TYPE_STRING1, 'a',
            TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0,
            TYPE_STRING1, 'z',
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<FloatEvent>(1.25),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY,
            TYPE_STRING1, 'a',
            TYPE_FLOAT16, 0xa0, 0x3f,
            TYPE_STRING1, 'z',
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<FloatEvent>(-5.923441e-50),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY,
            TYPE_STRING1, 'a',
            TYPE_FLOAT64, 0x35, 0x3c, 0xce, 0x81, 0x87, 0x29, 0xb6, 0xb5,
            TYPE_STRING1, 'z',
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<StringEvent>("b"),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY,
            TYPE_STRING1, 'a',
            TYPE_STRING1, 'b',
            TYPE_STRING1, 'z',
        TYPE_END,
    });

    assert_encode(
    {
        std::make_shared<ArrayBeginEvent>(),
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
        TYPE_ARRAY,
            TYPE_STRING1, 'a',
            TYPE_STRING, 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', STRING_TERMINATOR,
            TYPE_STRING1, 'z',
            0x01,
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<BooleanEvent>(false),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY,
            TYPE_STRING1, 'a',
            TYPE_FALSE,
            TYPE_STRING1, 'z',
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<NullEvent>(),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY,
            TYPE_STRING1, 'a',
            TYPE_NULL,
            TYPE_STRING1, 'z',
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY,
            TYPE_STRING1, 'a',
            TYPE_OBJECT,
            TYPE_END,
            TYPE_STRING1, 'z',
        TYPE_END,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        TYPE_ARRAY,
            TYPE_STRING1, 'a',
            TYPE_ARRAY,
            TYPE_END,
            TYPE_STRING1, 'z',
        TYPE_END,
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
    assert_encode_failure(
    {
        std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
    });

    assert_encode_failure(
    {
        std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
        std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
        std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
        std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
        std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
            std::make_shared<IntegerEvent>(1LL),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
        std::make_shared<ContainerEndEvent>(),
    });
}

TEST(Encoder, fail_containers)
{
    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ObjectBeginEvent>(),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ArrayBeginEvent>(),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ArrayBeginEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ArrayBeginEvent>(),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ObjectBeginEvent>(),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ObjectBeginEvent>(),
                std::make_shared<StringEvent>("a"),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
        std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ArrayBeginEvent>(),
        std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
    });
}

TEST(Decoder, unbalanced_containers)
{
    assert_decode_failure({TYPE_OBJECT});
    assert_decode_failure({TYPE_OBJECT, TYPE_OBJECT, TYPE_END});
    assert_decode_failure({TYPE_OBJECT, TYPE_ARRAY, TYPE_END});
    assert_decode_failure({TYPE_ARRAY});
    assert_decode_failure({TYPE_ARRAY, TYPE_ARRAY, TYPE_END});
    assert_decode_failure({TYPE_ARRAY, TYPE_OBJECT, TYPE_END});
}

TEST(Decoder, fail_big_number)
{
    assert_decode({TYPE_BIG_NUMBER, 0x02, 0x00}, {std::make_shared<UIntegerEvent>(0LL)});
}

TEST(Decoder, fail_big_number_length_field)
{
    assert_decode_failure({TYPE_BIG_NUMBER});
    assert_decode_failure({TYPE_BIG_NUMBER, 0x02});
}


TEST(Decoder, fail_float)
{
    assert_encode_failure({std::make_shared<FloatEvent>(NAN)});
    assert_encode_failure({std::make_shared<FloatEvent>(INFINITY)});
    assert_encode_failure({std::make_shared<FloatEvent>(-INFINITY)});
}


// ------------------------------------
// Example Tests
// ------------------------------------

TEST(Examples, specification)
{
    // Short String
    assert_encode_decode({std::make_shared<StringEvent>("")},  {0x80});
    assert_encode_decode({std::make_shared<StringEvent>("A")},  {0x81, 0x41});
    assert_encode_decode({std::make_shared<StringEvent>("おはよう")},  {0x8c, 0xe3, 0x81, 0x8a, 0xe3, 0x81, 0xaf, 0xe3, 0x82, 0x88, 0xe3, 0x81, 0x86});
    assert_encode_decode({std::make_shared<StringEvent>("15 byte string!")},  {0x8f, 0x31, 0x35, 0x20, 0x62, 0x79, 0x74, 0x65, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x21});

    // Long String
    assert_decode({0x90, 0xff}, {std::make_shared<StringEvent>("")});
    assert_decode({0x90, 0x61, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0xff}, {std::make_shared<StringEvent>("a string")});

    // Small Integer
    assert_encode_decode({std::make_shared<IntegerEvent>( 106)},  {0x6a});
    assert_encode_decode({std::make_shared<IntegerEvent>(   5)},  {0x05});
    assert_encode_decode({std::make_shared<IntegerEvent>(   0)},  {0x00});
    assert_encode_decode({std::make_shared<IntegerEvent>( -60)},  {0xc4});
    assert_encode_decode({std::make_shared<IntegerEvent>(-100)},  {0x9c});
    assert_encode_decode({std::make_shared<IntegerEvent>(-105)},  {0x97});

    // Integer
    assert_decode({0x71, 0xe8, 0x03}, {std::make_shared<IntegerEvent>(1000)});
    assert_decode({0x79, 0x18, 0xfc}, {std::make_shared<IntegerEvent>(-1000)});
    assert_decode({0x71, 0x00, 0x80}, {std::make_shared<IntegerEvent>(0x8000)});
    assert_decode({0x75, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12}, {std::make_shared<IntegerEvent>(0x123456789abcLL)});
    assert_decode({0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80}, {std::make_shared<IntegerEvent>(-0x8000000000000000LL)});
    assert_decode({0x77, 0xda, 0xda, 0xda, 0xde, 0xd0, 0xd0, 0xd0, 0xde}, {std::make_shared<UIntegerEvent>(0xded0d0d0dedadada)});

    // 16 Bit Float
    assert_encode_decode({std::make_shared<FloatEvent>(1.125)},  {0x6b, 0x90, 0x3f});

    // 32 Bit Float
    assert_encode_decode({std::make_shared<FloatEvent>(0x1.3f7p5)},  {0x6c, 0x00, 0xb8, 0x1f, 0x42});

    // 64 Bit Float
    assert_encode_decode({std::make_shared<FloatEvent>(1.234)},  {0x6d, 0x58, 0x39, 0xb4, 0xc8, 0x76, 0xbe, 0xf3, 0x3f});

    // Array
    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1),
            std::make_shared<NullEvent>(),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x81, 0x61,
            0x01,
            0x95,
        0x94,
    });

    // Object
    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("b"),
            std::make_shared<IntegerEvent>(0),
            std::make_shared<StringEvent>("test"),
            std::make_shared<StringEvent>("x"),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x93,
            0x81, 0x62,
            0x00,
            0x84, 0x74, 0x65, 0x73, 0x74,
            0x81, 0x78,
        0x94,
    });

    // Boolean
    assert_encode_decode({std::make_shared<BooleanEvent>(false)}, {0x6e});
    assert_encode_decode({std::make_shared<BooleanEvent>(true)}, {0x6f});

    // Null
    assert_encode_decode({std::make_shared<NullEvent>()}, {0x95});

    // Full Example
    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a number"),
            std::make_shared<IntegerEvent>(1LL),
            std::make_shared<StringEvent>("an array"),
            std::make_shared<ArrayBeginEvent>(),
                std::make_shared<StringEvent>("x"),
                std::make_shared<IntegerEvent>(1000LL),
                std::make_shared<FloatEvent>(1.5),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("a null"),
            std::make_shared<NullEvent>(),
            std::make_shared<StringEvent>("a boolean"),
            std::make_shared<BooleanEvent>(true),
            std::make_shared<StringEvent>("an object"),
            std::make_shared<ObjectBeginEvent>(),
                std::make_shared<StringEvent>("a"),
                std::make_shared<IntegerEvent>(-100LL),
                std::make_shared<StringEvent>("b"),
                std::make_shared<StringEvent>("........................................"),
            std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x93,
            0x88, 0x61, 0x20, 0x6e, 0x75, 0x6d, 0x62, 0x65, 0x72,
            0x01,
            0x88, 0x61, 0x6e, 0x20, 0x61, 0x72, 0x72, 0x61, 0x79,
            0x92,
                0x81, 0x78,
                0x79, 0xe8, 0x03,
                0x6b, 0xc0, 0x3f,
            0x94,
            0x86, 0x61, 0x20, 0x6e, 0x75, 0x6c, 0x6c,
            0x95,
            0x89, 0x61, 0x20, 0x62, 0x6f, 0x6f, 0x6c, 0x65, 0x61, 0x6e,
            0x6f,
            0x89, 0x61, 0x6e, 0x20, 0x6f, 0x62, 0x6a, 0x65, 0x63, 0x74,
            0x93,
                0x81, 0x61,
                0x9c,
                0x81, 0x62,
                0x90, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
                      0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
                      0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
                      0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
                      0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
                0xff,
            0x94,
        0x94,
    });
}
