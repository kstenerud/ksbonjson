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

// ABOUTME: Tests for BONJSON encoder/decoder with new delimiter-terminated
// ABOUTME: container format and CPU-native integer sizes.

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
// 0xc9: reserved
// 0xca: BigNumber (zigzag LEB128)
// 0xcb: Float32, 0xcc: Float64
// 0xcd: Null, 0xce: False, 0xcf: True
// Short strings: 0xd0-0xdf (0-15 bytes)
// Unsigned integers: 0xe0-0xe3 (1, 2, 4, 8 bytes CPU-native)
// Signed integers: 0xe4-0xe7 (1, 2, 4, 8 bytes CPU-native)
// 0xfc: Array, 0xfd: Object (terminated by 0xfe)
// 0xfe: End container
// 0xff: Long string delimiter (0xff + data + 0xff)
enum
{
    // Big number
    TYPE_BIG_NUMBER = 0xca,

    // Floats
    TYPE_FLOAT32    = 0xcb,
    TYPE_FLOAT64    = 0xcc,

    // Null, Boolean
    TYPE_NULL       = 0xcd,
    TYPE_FALSE      = 0xce,
    TYPE_TRUE       = 0xcf,

    // Short strings: 0xd0-0xdf
    TYPE_STRING0  = 0xd0,
    TYPE_STRING1  = 0xd1,
    TYPE_STRING2  = 0xd2,
    TYPE_STRING3  = 0xd3,
    TYPE_STRING4  = 0xd4,
    TYPE_STRING5  = 0xd5,
    TYPE_STRING6  = 0xd6,
    TYPE_STRING7  = 0xd7,
    TYPE_STRING8  = 0xd8,
    TYPE_STRING9  = 0xd9,
    TYPE_STRING10 = 0xda,
    TYPE_STRING11 = 0xdb,
    TYPE_STRING12 = 0xdc,
    TYPE_STRING13 = 0xdd,
    TYPE_STRING14 = 0xde,
    TYPE_STRING15 = 0xdf,

    // Unsigned integers: 0xe0-0xe3 (CPU-native: 1, 2, 4, 8 bytes)
    TYPE_UINT8  = 0xe0,
    TYPE_UINT16 = 0xe1,
    TYPE_UINT32 = 0xe2,
    TYPE_UINT64 = 0xe3,

    // Signed integers: 0xe4-0xe7 (CPU-native: 1, 2, 4, 8 bytes)
    TYPE_SINT8  = 0xe4,
    TYPE_SINT16 = 0xe5,
    TYPE_SINT32 = 0xe6,
    TYPE_SINT64 = 0xe7,

    // Containers
    TYPE_ARRAY  = 0xfc,
    TYPE_OBJECT = 0xfd,

    // Container end marker
    TYPE_END = 0xfe,

    // Long string delimiter
    TYPE_STRINGL = 0xff,
};

enum
{
    SMALLINT_MIN = -100,
    SMALLINT_MAX = 100,
    SMALLINT_BIAS = 100,  // type_code = value + SMALLINT_BIAS
};

// Helper to convert small int value to type code
#define SMALLINT(v) ((uint8_t)((v) + SMALLINT_BIAS))

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

TEST(EncodeDecode, float32)
{
    // 1.125 fits in float32
    assert_encode_decode({std::make_shared<FloatEvent>(1.125)}, {TYPE_FLOAT32, 0x00, 0x00, 0x90, 0x3f});

    // 0x1.3f7p5 fits in float32
    assert_encode_decode({std::make_shared<FloatEvent>(0x1.3f7p5)}, {TYPE_FLOAT32, 0x00, 0xb8, 0x1f, 0x42});
}

TEST(EncodeDecode, float64)
{
    assert_encode_decode({std::make_shared<FloatEvent>(1.234)}, {TYPE_FLOAT64, 0x58, 0x39, 0xb4, 0xc8, 0x76, 0xbe, 0xf3, 0x3f});

    // Decoding a float64-encoded 1.125 should produce same float event
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
    // Values 101-127 fit in signed 1-byte
    assert_encode_decode({std::make_shared<IntegerEvent>( 127)}, {TYPE_SINT8, (uint8_t)( 127)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 101)}, {TYPE_SINT8, (uint8_t)( 101)});

    // Values 128-255 need unsigned 1-byte
    assert_encode_decode({std::make_shared<IntegerEvent>( 128)}, {TYPE_UINT8, (uint8_t)( 128)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 255)}, {TYPE_UINT8, (uint8_t)( 255)});

    // Values -101 to -128 fit in signed 1-byte
    assert_encode_decode({std::make_shared<IntegerEvent>(-101)}, {TYPE_SINT8, (uint8_t)(-101)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-128)}, {TYPE_SINT8, (uint8_t)(-128)});
}

TEST(EncodeDecode, int16)
{
    // Values that need 2 bytes signed
    assert_encode_decode({std::make_shared<IntegerEvent>(   1000LL)}, {TYPE_SINT16, 0xe8, 0x03});
    assert_encode_decode({std::make_shared<IntegerEvent>(  0x100LL)}, {TYPE_SINT16, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>( 0x7fffLL)}, {TYPE_SINT16, 0xff, 0x7f});

    // Values that need 2 bytes unsigned (MSB set in 16 bits)
    assert_encode_decode({std::make_shared<IntegerEvent>( 0x8000LL)}, {TYPE_UINT16, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>( 0xffffLL)}, {TYPE_UINT16, 0xff, 0xff});

    // Negative values that need 2 bytes signed
    assert_encode_decode({std::make_shared<IntegerEvent>( -0x81LL)}, {TYPE_SINT16, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000LL)}, {TYPE_SINT16, 0x00, 0x80});

    // Decode: signed 16-bit with small value
    assert_decode({std::make_shared<IntegerEvent>(50)}, {TYPE_SINT16, 50, 0});
}

TEST(EncodeDecode, int32)
{
    // Values that need 4 bytes signed (3-byte values round up to 4)
    assert_encode_decode({std::make_shared<IntegerEvent>(  0x10000LL)}, {TYPE_SINT32, 0x00, 0x00, 0x01, 0x00});
    assert_encode_decode({std::make_shared<IntegerEvent>( 0x7fffffffLL)}, {TYPE_SINT32, 0xff, 0xff, 0xff, 0x7f});

    // Values that need 4 bytes unsigned
    assert_encode_decode({std::make_shared<IntegerEvent>( 0x80000000LL)}, {TYPE_UINT32, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>( 0xffffffffLL)}, {TYPE_UINT32, 0xff, 0xff, 0xff, 0xff});

    // Negative values that need 4 bytes
    assert_encode_decode({std::make_shared<IntegerEvent>( -0x8001LL)}, {TYPE_SINT32, 0xff, 0x7f, 0xff, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000000LL)}, {TYPE_SINT32, 0x00, 0x00, 0x00, 0x80});
}

TEST(EncodeDecode, int64)
{
    // Values that need 8 bytes signed (5-7 byte values round up to 8)
    assert_encode_decode({std::make_shared<IntegerEvent>( 0x100000000LL)}, {TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00});
    assert_encode_decode({std::make_shared<IntegerEvent>( 0x7fffffffffffffffLL)}, {TYPE_SINT64, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});

    // Values that need 8 bytes unsigned
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x8000000000000000ULL)}, {TYPE_UINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xffffffffffffffffULL)}, {TYPE_UINT64, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});

    // Negative values that need 8 bytes
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000001LL)}, {TYPE_SINT64, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff});

    // INT64_MIN
    assert_encode_decode({std::make_shared<IntegerEvent>((int64_t)0x8000000000000000LL)}, {TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
}

// ------------------------------------
// String Tests
// ------------------------------------

TEST(EncodeDecode, short_string_empty)
{
    assert_encode_decode({std::make_shared<StringEvent>("")}, {TYPE_STRING0});
}

TEST(EncodeDecode, short_string)
{
    assert_encode_decode({std::make_shared<StringEvent>("a")}, {TYPE_STRING1, 'a'});
    assert_encode_decode({std::make_shared<StringEvent>("ab")}, {TYPE_STRING2, 'a', 'b'});
    assert_encode_decode({std::make_shared<StringEvent>("test")}, {TYPE_STRING4, 't', 'e', 's', 't'});

    // Max short string: 15 bytes
    assert_encode_decode(
        {std::make_shared<StringEvent>("123456789012345")},
        {TYPE_STRING15, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5'}
    );
}

TEST(EncodeDecode, long_string)
{
    // 16 bytes is too long for short string, uses long string format: 0xFF + data + 0xFF
    assert_encode_decode(
        {std::make_shared<StringEvent>("1234567890123456")},
        {TYPE_STRINGL, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', TYPE_STRINGL}
    );
}

// ------------------------------------
// Big Number Tests
// ------------------------------------

TEST(EncodeDecode, big_number_zero)
{
    // BigNumber(+, 0, exp=0): 0xCA + zigzag_leb128(0) + zigzag_leb128(0) = CA 00 00
    assert_encode_decode(
        {std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(1, 0, 0))},
        {TYPE_BIG_NUMBER, 0x00, 0x00}
    );
}

TEST(EncodeDecode, big_number_positive)
{
    // BigNumber(+, 123, exp=0): 0xCA + zigzag_leb128(0) + zigzag_leb128(123)
    // zigzag(0) = 0 -> LEB128 = 0x00
    // zigzag(123) = 246 -> LEB128 = 0xf6, 0x01
    assert_encode_decode(
        {std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(1, 123, 0))},
        {TYPE_BIG_NUMBER, 0x00, 0xf6, 0x01}
    );
}

TEST(EncodeDecode, big_number_negative)
{
    // BigNumber(-, 123, exp=0): 0xCA + zigzag_leb128(0) + zigzag_leb128(-123)
    // zigzag(-123) = 245 -> LEB128 = 0xf5, 0x01
    assert_encode_decode(
        {std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(-1, 123, 0))},
        {TYPE_BIG_NUMBER, 0x00, 0xf5, 0x01}
    );
}

TEST(EncodeDecode, big_number_with_exponent)
{
    // BigNumber(+, 1, exp=5): 0xCA + zigzag_leb128(5) + zigzag_leb128(1)
    // zigzag(5) = 10 -> LEB128 = 0x0a
    // zigzag(1) = 2 -> LEB128 = 0x02
    assert_encode_decode(
        {std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(1, 1, 5))},
        {TYPE_BIG_NUMBER, 0x0a, 0x02}
    );
}

TEST(EncodeDecode, big_number_negative_exponent)
{
    // BigNumber(+, 1, exp=-3): 0xCA + zigzag_leb128(-3) + zigzag_leb128(1)
    // zigzag(-3) = 5 -> LEB128 = 0x05
    // zigzag(1) = 2 -> LEB128 = 0x02
    assert_encode_decode(
        {std::make_shared<BigNumberEvent>(ksbonjson_newBigNumber(1, 1, -3))},
        {TYPE_BIG_NUMBER, 0x05, 0x02}
    );
}

// ------------------------------------
// Container Tests
// ------------------------------------

TEST(EncodeDecode, empty_array)
{
    // Empty array: TYPE_ARRAY TYPE_END
    assert_encode_decode(
        {std::make_shared<ArrayBeginEvent>(), std::make_shared<ContainerEndEvent>()},
        {TYPE_ARRAY, TYPE_END}
    );
}

TEST(EncodeDecode, empty_object)
{
    // Empty object: TYPE_OBJECT TYPE_END
    assert_encode_decode(
        {std::make_shared<ObjectBeginEvent>(), std::make_shared<ContainerEndEvent>()},
        {TYPE_OBJECT, TYPE_END}
    );
}

TEST(EncodeDecode, array_with_elements)
{
    // [1, 2, 3]: TYPE_ARRAY SMALLINT(1) SMALLINT(2) SMALLINT(3) TYPE_END
    assert_encode_decode(
        {
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<IntegerEvent>(1),
            std::make_shared<IntegerEvent>(2),
            std::make_shared<IntegerEvent>(3),
            std::make_shared<ContainerEndEvent>()
        },
        {TYPE_ARRAY, SMALLINT(1), SMALLINT(2), SMALLINT(3), TYPE_END}
    );
}

TEST(EncodeDecode, object_with_elements)
{
    // {"a": 1}: TYPE_OBJECT TYPE_STRING1 'a' SMALLINT(1) TYPE_END
    assert_encode_decode(
        {
            std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1),
            std::make_shared<ContainerEndEvent>()
        },
        {TYPE_OBJECT, TYPE_STRING1, 'a', SMALLINT(1), TYPE_END}
    );
}

TEST(EncodeDecode, nested_containers)
{
    // {"a": [1, 2]}: TYPE_OBJECT STRING1 'a' TYPE_ARRAY SMALLINT(1) SMALLINT(2) TYPE_END TYPE_END
    assert_encode_decode(
        {
            std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<IntegerEvent>(1),
            std::make_shared<IntegerEvent>(2),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<ContainerEndEvent>()
        },
        {TYPE_OBJECT, TYPE_STRING1, 'a', TYPE_ARRAY, SMALLINT(1), SMALLINT(2), TYPE_END, TYPE_END}
    );
}

TEST(EncodeDecode, deeply_nested)
{
    // [[[]]]
    assert_encode_decode(
        {
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<ContainerEndEvent>()
        },
        {TYPE_ARRAY, TYPE_ARRAY, TYPE_ARRAY, TYPE_END, TYPE_END, TYPE_END}
    );
}

TEST(EncodeDecode, object_multiple_pairs)
{
    // {"a": 1, "b": 2}
    assert_encode_decode(
        {
            std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1),
            std::make_shared<StringEvent>("b"),
            std::make_shared<IntegerEvent>(2),
            std::make_shared<ContainerEndEvent>()
        },
        {TYPE_OBJECT, TYPE_STRING1, 'a', SMALLINT(1), TYPE_STRING1, 'b', SMALLINT(2), TYPE_END}
    );
}

TEST(EncodeDecode, mixed_types_in_array)
{
    // [null, true, false, 42, "hello"]
    assert_encode_decode(
        {
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<NullEvent>(),
            std::make_shared<BooleanEvent>(true),
            std::make_shared<BooleanEvent>(false),
            std::make_shared<IntegerEvent>(42),
            std::make_shared<StringEvent>("hello"),
            std::make_shared<ContainerEndEvent>()
        },
        {TYPE_ARRAY, TYPE_NULL, TYPE_TRUE, TYPE_FALSE, SMALLINT(42), TYPE_STRING5, 'h', 'e', 'l', 'l', 'o', TYPE_END}
    );
}

// ------------------------------------
// Float as Int Tests
// ------------------------------------

TEST(EncodeDecode, float_as_int)
{
    // Float values that are exact integers should be encoded as integers
    assert_encode_decode({std::make_shared<FloatEvent>(0.0)}, {SMALLINT(0)});
    assert_encode_decode({std::make_shared<FloatEvent>(1.0)}, {SMALLINT(1)});
    assert_encode_decode({std::make_shared<FloatEvent>(-1.0)}, {SMALLINT(-1)});
    assert_encode_decode({std::make_shared<FloatEvent>(100.0)}, {SMALLINT(100)});
    assert_encode_decode({std::make_shared<FloatEvent>(1000.0)}, {TYPE_SINT16, 0xe8, 0x03});
}

// ------------------------------------
// Error Tests
// ------------------------------------

TEST(EncodeError, containers_still_open)
{
    assert_encode_result(KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN,
        {std::make_shared<ArrayBeginEvent>()});
}

TEST(EncodeError, closed_too_many_containers)
{
    assert_encode_result(KSBONJSON_ENCODE_CLOSED_TOO_MANY_CONTAINERS,
        {std::make_shared<ContainerEndEvent>()});
}

TEST(EncodeError, expected_object_name)
{
    // Trying to add an integer when an object key is expected
    assert_encode_result(KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME,
        {
            std::make_shared<ObjectBeginEvent>(),
            std::make_shared<IntegerEvent>(1)
        });
}

TEST(EncodeError, expected_object_value)
{
    // Trying to close object after providing key but not value
    assert_encode_result(KSBONJSON_ENCODE_EXPECTED_OBJECT_VALUE,
        {
            std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("key"),
            std::make_shared<ContainerEndEvent>()
        });
}

TEST(EncodeError, invalid_float_nan)
{
    assert_encode_result(KSBONJSON_ENCODE_INVALID_DATA,
        {std::make_shared<FloatEvent>(NAN)});
}

TEST(EncodeError, invalid_float_inf)
{
    assert_encode_result(KSBONJSON_ENCODE_INVALID_DATA,
        {std::make_shared<FloatEvent>(INFINITY)});
}

// ------------------------------------
// Decode Error Tests
// ------------------------------------

TEST(DecodeError, empty_document)
{
    assert_decode_result(KSBONJSON_DECODE_EMPTY_DOCUMENT, {});
}

TEST(DecodeError, truncated_int8)
{
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_SINT8});
}

TEST(DecodeError, truncated_int16)
{
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_SINT16, 0x01});
}

TEST(DecodeError, truncated_float32)
{
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_FLOAT32, 0x00, 0x00});
}

TEST(DecodeError, truncated_float64)
{
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_FLOAT64, 0x00, 0x00, 0x00, 0x00});
}

TEST(DecodeError, unclosed_array)
{
    assert_decode_result(KSBONJSON_DECODE_UNCLOSED_CONTAINERS, {TYPE_ARRAY});
}

TEST(DecodeError, unclosed_object)
{
    assert_decode_result(KSBONJSON_DECODE_UNCLOSED_CONTAINERS, {TYPE_OBJECT});
}

TEST(DecodeError, unbalanced_end)
{
    // TYPE_END at top level
    assert_decode_result(KSBONJSON_DECODE_UNBALANCED_CONTAINERS, {TYPE_END});
}

TEST(DecodeError, trailing_data)
{
    assert_decode_result(KSBONJSON_DECODE_TRAILING_DATA, {TYPE_NULL, TYPE_NULL});
}

TEST(DecodeError, expected_object_name)
{
    // Object with non-string key
    assert_decode_result(KSBONJSON_DECODE_EXPECTED_OBJECT_NAME, {TYPE_OBJECT, SMALLINT(1), TYPE_END});
}

TEST(DecodeError, expected_object_value)
{
    // Object with key but no value before end
    assert_decode_result(KSBONJSON_DECODE_EXPECTED_OBJECT_VALUE, {TYPE_OBJECT, TYPE_STRING1, 'a', TYPE_END});
}

TEST(DecodeError, reserved_type_code)
{
    assert_decode_result(KSBONJSON_DECODE_INVALID_DATA, {0xc9});
}

TEST(DecodeError, truncated_long_string)
{
    // Long string without terminating 0xFF
    assert_decode_result(KSBONJSON_DECODE_INCOMPLETE, {TYPE_STRINGL, 'a', 'b', 'c'});
}

// ------------------------------------
// Unsigned Integer Tests
// ------------------------------------

TEST(EncodeDecode, uint_smallint_range)
{
    // Unsigned values in small int range should use small int encoding
    assert_encode_decode({std::make_shared<UIntegerEvent>(0ULL)}, {SMALLINT(0)});
    assert_encode_decode({std::make_shared<UIntegerEvent>(100ULL)}, {SMALLINT(100)});
}

TEST(EncodeDecode, uint8)
{
    assert_encode_decode({std::make_shared<UIntegerEvent>(128ULL)}, {TYPE_UINT8, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(255ULL)}, {TYPE_UINT8, 0xff});
}

TEST(EncodeDecode, uint16)
{
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x8000ULL)}, {TYPE_UINT16, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xffffULL)}, {TYPE_UINT16, 0xff, 0xff});
}

TEST(EncodeDecode, uint32)
{
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x80000000ULL)}, {TYPE_UINT32, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xffffffffULL)}, {TYPE_UINT32, 0xff, 0xff, 0xff, 0xff});
}

TEST(EncodeDecode, uint64)
{
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x8000000000000000ULL)}, {TYPE_UINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xffffffffffffffffULL)}, {TYPE_UINT64, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
}

// Values that fit in signed should prefer signed type
TEST(EncodeDecode, uint_prefers_signed)
{
    // 101 fits in signed 1-byte (MSB clear)
    assert_encode_decode({std::make_shared<UIntegerEvent>(101ULL)}, {TYPE_SINT8, 101});
    // 0x100 fits in signed 2-byte
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x100ULL)}, {TYPE_SINT16, 0x00, 0x01});
    // 0x10000 fits in signed 4-byte
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x10000ULL)}, {TYPE_SINT32, 0x00, 0x00, 0x01, 0x00});
    // 0x100000000 fits in signed 8-byte
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x100000000ULL)}, {TYPE_SINT64, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00});
}

// ------------------------------------
// Failing Encoder Tests
// ------------------------------------

TEST(EncoderError, addEncodedData_fails)
{
    KSBONJSONEncodeContext context;
    ksbonjson_beginEncode(&context, addEncodedDataFailCallback, nullptr);
    ASSERT_EQ(KSBONJSON_ENCODE_COULD_NOT_ADD_DATA, ksbonjson_addNull(&context));
}

// ------------------------------------
// Status description tests
// ------------------------------------

TEST(StatusDescriptions, encode)
{
    ASSERT_STRNE("", ksbonjson_describeEncodeStatus(KSBONJSON_ENCODE_OK));
    ASSERT_STRNE("", ksbonjson_describeEncodeStatus(KSBONJSON_ENCODE_EXPECTED_OBJECT_NAME));
    ASSERT_STRNE("", ksbonjson_describeEncodeStatus(KSBONJSON_ENCODE_EXPECTED_OBJECT_VALUE));
    ASSERT_STRNE("", ksbonjson_describeEncodeStatus(KSBONJSON_ENCODE_NULL_POINTER));
    ASSERT_STRNE("", ksbonjson_describeEncodeStatus(KSBONJSON_ENCODE_CLOSED_TOO_MANY_CONTAINERS));
    ASSERT_STRNE("", ksbonjson_describeEncodeStatus(KSBONJSON_ENCODE_CONTAINERS_ARE_STILL_OPEN));
    ASSERT_STRNE("", ksbonjson_describeEncodeStatus(KSBONJSON_ENCODE_INVALID_DATA));
    ASSERT_STRNE("", ksbonjson_describeEncodeStatus(KSBONJSON_ENCODE_COULD_NOT_ADD_DATA));
}

TEST(StatusDescriptions, decode)
{
    ASSERT_STRNE("", ksbonjson_describeDecodeStatus(KSBONJSON_DECODE_OK));
    ASSERT_STRNE("", ksbonjson_describeDecodeStatus(KSBONJSON_DECODE_INCOMPLETE));
    ASSERT_STRNE("", ksbonjson_describeDecodeStatus(KSBONJSON_DECODE_UNCLOSED_CONTAINERS));
    ASSERT_STRNE("", ksbonjson_describeDecodeStatus(KSBONJSON_DECODE_UNBALANCED_CONTAINERS));
    ASSERT_STRNE("", ksbonjson_describeDecodeStatus(KSBONJSON_DECODE_CONTAINER_DEPTH_EXCEEDED));
    ASSERT_STRNE("", ksbonjson_describeDecodeStatus(KSBONJSON_DECODE_EXPECTED_OBJECT_NAME));
    ASSERT_STRNE("", ksbonjson_describeDecodeStatus(KSBONJSON_DECODE_EXPECTED_OBJECT_VALUE));
    ASSERT_STRNE("", ksbonjson_describeDecodeStatus(KSBONJSON_DECODE_INVALID_DATA));
    ASSERT_STRNE("", ksbonjson_describeDecodeStatus(KSBONJSON_DECODE_COULD_NOT_PROCESS_DATA));
}
