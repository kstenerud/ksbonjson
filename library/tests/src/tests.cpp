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
#include <algorithm>

#include <ksbonjson/KSBONJSONEncoder.h>
#include <ksbonjson/KSBONJSONDecoder.h>


#define REPORT_DECODING false
#define REPORT_ENCODING false


#define MARK_UNUSED(x) (void)(x)

// ============================================================================
// Events
// ============================================================================

typedef enum
{
    CHUNK_HAS_NEXT,
    CHUNK_LAST
} CHUNK_MODE;

class Event
{
public:
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) = 0;
    virtual std::string description() const = 0;
    virtual std::string comparator() const = 0;
    virtual ~Event() {}
protected:
    friend bool operator==(const Event&, const Event&);
    virtual bool isEqual(const Event&) const {return true;};
};

bool operator==(const Event& lhs, const Event& rhs) {
    return lhs.comparator() == rhs.comparator();
    //return typeid(lhs) == typeid(rhs) && lhs.isEqual(rhs);
}
bool operator!=(const Event& lhs, const Event& rhs) {
    return !(lhs == rhs);
}
std::ostream &operator<<(std::ostream &os, Event const &e) { 
    return os << e.description();
}
std::ostream &operator<<(std::ostream &os, std::vector<std::shared_ptr<Event>> &v) { 
    os << "[";
    for(std::shared_ptr<Event> &e: v)
    {
        os << e->description() << " ";
    }
    return os << "]";
}

class BooleanEvent: public Event
{
public:
    BooleanEvent(bool value)
    : value(value)
    {}
    virtual ~BooleanEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_addBoolean(ctx, value);
    }
    virtual std::string comparator() const override
    {
        std::ostringstream str;
        str << (value ? "true" : "false");
        return str.str();
    }
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "B(" << (value ? "true" : "false") << ")";
        return str.str();
    }
private:
    bool value;
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const BooleanEvent&>(obj);
        return Event::isEqual(v) && v.value == value;
    }
};

class IntegerEvent: public Event
{
public:
    IntegerEvent(int64_t value)
    : value(value)
    {}
    virtual ~IntegerEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_addInteger(ctx, value);
    }
    virtual std::string comparator() const override
    {
        std::ostringstream str;
        str << value;
        return str.str();
    }
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "I(" << value << ")";
        return str.str();
    }
private:
    int64_t value;
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const IntegerEvent&>(obj);
        return Event::isEqual(v) && v.value == value;
    }
};

class UIntegerEvent: public Event
{
public:
    UIntegerEvent(uint64_t value)
    : value(value)
    {}
    virtual ~UIntegerEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_addUInteger(ctx, value);
    }
    virtual std::string comparator() const override
    {
        std::ostringstream str;
        str << value;
        return str.str();
    }
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "U(" << value << ")";
        return str.str();
    }
private:
    uint64_t value;
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const UIntegerEvent&>(obj);
        return Event::isEqual(v) && v.value == value;
    }
};

class FloatEvent: public Event
{
public:
    FloatEvent(double value)
    : value(value)
    {}
    virtual ~FloatEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_addFloat(ctx, value);
    }
    virtual std::string comparator() const override
    {
        std::ostringstream str;
        str << value;
        return str.str();
    }
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "F(" << value << ")";
        return str.str();
    }
private:
    double value;
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const FloatEvent&>(obj);
        return Event::isEqual(v) && v.value == value;
    }
};

class NullEvent: public Event
{
public:
    virtual ~NullEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_addNull(ctx);
    }
    virtual std::string comparator() const override
    {
        return "null";
    }
    virtual std::string description() const override
    {
        return "N()";
    }
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const NullEvent&>(obj);
        return Event::isEqual(v);
    }
};

class StringEvent: public Event
{
public:
    StringEvent(std::string value)
    : value(value)
    {}
    StringEvent(const char* value)
    : value(value)
    {}
    StringEvent(const char* value, size_t length)
    : value(value, length)
    {}
    virtual ~StringEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_addString(ctx, value.c_str(), value.length());
    }
    virtual std::string comparator() const override
    {
        std::ostringstream str;
        str << "\"" << value << "\"";
        return str.str();
    }
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "S(" << value << ")";
        return str.str();
    }
private:
    std::string value;
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const StringEvent&>(obj);
        return Event::isEqual(v) && v.value == value;
    }
};

class StringChunkEvent: public Event
{
public:
    StringChunkEvent(std::string value, CHUNK_MODE chunkMode)
    : value(value)
    , chunkMode(chunkMode)
    {}
    StringChunkEvent(const char* value, CHUNK_MODE chunkMode)
    : value(value)
    , chunkMode(chunkMode)
    {}
    StringChunkEvent(const char* value, size_t length, CHUNK_MODE chunkMode)
    : value(value, length)
    , chunkMode(chunkMode)
    {}
    virtual ~StringChunkEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_chunkString(ctx, value.c_str(), value.length(), chunkMode == CHUNK_LAST);
    }
    virtual std::string comparator() const override
    {
        std::ostringstream str;
        str << "'" << value << "'";
        return str.str();
    }
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "C(" << value << ")";
        return str.str();
    }
private:
    std::string value;
    CHUNK_MODE chunkMode;
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const StringChunkEvent&>(obj);
        return Event::isEqual(v) && (v.value == value) && (v.chunkMode = chunkMode);
    }
};

class BONJSONDocumentEvent: public Event
{
public:
    BONJSONDocumentEvent(std::vector<uint8_t> value)
    : value(value)
    {}
    BONJSONDocumentEvent(const uint8_t* value, size_t length)
    : value(value, value+length)
    {}
    virtual ~BONJSONDocumentEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_addBONJSONDocument(ctx, value.data(), value.size());
    }
    virtual std::string comparator() const override
    {
        std::ostringstream str;
        str << "(" << value.size() << ")";
        return str.str();
    }
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "BON(" << value.size() << ")";
        return str.str();
    }
private:
    std::vector<uint8_t> value;
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const BONJSONDocumentEvent&>(obj);
        return Event::isEqual(v) && v.value == value;
    }
};

class ObjectBeginEvent: public Event
{
public:
    virtual ~ObjectBeginEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_beginObject(ctx);
    }
    virtual std::string comparator() const override
    {
        return "obj";
    }
    virtual std::string description() const override
    {
        return "O()";
    }
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const ObjectBeginEvent&>(obj);
        return Event::isEqual(v);
    }
};

class ArrayBeginEvent: public Event
{
public:
    virtual ~ArrayBeginEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_beginArray(ctx);
    }
    virtual std::string comparator() const override
    {
        return "arr";
    }
    virtual std::string description() const override
    {
        return "A()";
    }
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const ArrayBeginEvent&>(obj);
        return Event::isEqual(v);
    }
};

class ContainerEndEvent: public Event
{
public:
    virtual ~ContainerEndEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_endContainer(ctx);
    }
    virtual std::string comparator() const override
    {
        return "end";
    }
    virtual std::string description() const override
    {
        return "E()";
    }
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const ContainerEndEvent&>(obj);
        return Event::isEqual(v);
    }
};


// ============================================================================
// Decoder Context
// ============================================================================

class DecoderContext
{
public:
    DecoderContext();
    void addEvent(std::shared_ptr<Event> event)
    {
        if(REPORT_DECODING)
        {
            printf("%s\n", event->description().c_str());
        }
        events.push_back(event);
    }
public:
    KSBONJSONDecodeCallbacks callbacks;
    std::vector<std::shared_ptr<Event>> events;
};

static ksbonjson_decodeStatus onBoolean(bool value, void* userData)
{
    DecoderContext* ctx = (DecoderContext*)userData;
    ctx->addEvent(std::make_shared<BooleanEvent>(value));
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onInteger(int64_t value, void* userData)
{
    DecoderContext* ctx = (DecoderContext*)userData;
    ctx->addEvent(std::make_shared<IntegerEvent>(value));
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onUInteger(uint64_t value, void* userData)
{
    DecoderContext* ctx = (DecoderContext*)userData;
    ctx->addEvent(std::make_shared<UIntegerEvent>(value));
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onFloat(double value, void* userData)
{
    DecoderContext* ctx = (DecoderContext*)userData;
    ctx->addEvent(std::make_shared<FloatEvent>(value));
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onNull(void* userData)
{
    DecoderContext* ctx = (DecoderContext*)userData;
    ctx->addEvent(std::make_shared<NullEvent>());
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onString(const char* KSBONJSON_RESTRICT value,
                                       size_t length,
                                       void* KSBONJSON_RESTRICT userData)
{
    DecoderContext* ctx = (DecoderContext*)userData;
    ctx->addEvent(std::make_shared<StringEvent>(value, length));
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onBeginObject(void* userData)
{
    DecoderContext* ctx = (DecoderContext*)userData;
    ctx->addEvent(std::make_shared<ObjectBeginEvent>());
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onBeginArray(void* userData)
{
    DecoderContext* ctx = (DecoderContext*)userData;
    ctx->addEvent(std::make_shared<ArrayBeginEvent>());
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onEndContainer(void* userData)
{
    DecoderContext* ctx = (DecoderContext*)userData;
    ctx->addEvent(std::make_shared<ContainerEndEvent>());
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onEndData(void* userData)
{
    MARK_UNUSED(userData);
    return KSBONJSON_DECODE_OK;
}

DecoderContext::DecoderContext()
: callbacks
{
    .onBoolean = onBoolean,
    .onInteger = onInteger,
    .onUInteger = onUInteger,
    .onFloat = onFloat,
    .onNull = onNull,
    .onString = onString,
    .onBeginObject = onBeginObject,
    .onBeginArray = onBeginArray,
    .onEndContainer = onEndContainer,
    .onEndData = onEndData,
}
{}


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
    if(REPORT_ENCODING)
    {
        static const char hexDigits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                         '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
        for(size_t i = 0; i < dataLength; i++)
        {
            uint8_t ch = data[i];
            uint8_t hi = ch >> 4;
            uint8_t lo = ch & 15;
            printf("%c%c ", hexDigits[hi], hexDigits[lo]);
        }
        printf("\n");
    }
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
    memset(&eContext, 0, sizeof(eContext));
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
    DecoderContext dCtx;
    size_t decodedOffset = 0;
    std::vector<uint8_t> document = actual_encoded;
    ASSERT_EQ(KSBONJSON_DECODE_OK, ksbonjson_decode(document.data(), document.size(), &dCtx.callbacks, &dCtx, &decodedOffset));
    ASSERT_EQ(expected_encoded.size(), decodedOffset);
    assert_events_equal(events, dCtx.events);

    // Encode again
    if(REPORT_ENCODING)
    {
        printf("\n[assert_encode_decode]: Encode again\n");
    }
    eCtx.reset();
    memset(&eContext, 0, sizeof(eContext));
    ksbonjson_beginEncode(&eContext, addEncodedDataCallback, &eCtx);
    for (const std::shared_ptr<Event>& event: dCtx.events)
    {
        ASSERT_EQ(KSBONJSON_ENCODE_OK, (*event)(&eContext));
    }
    ASSERT_EQ(KSBONJSON_ENCODE_OK, ksbonjson_endEncode(&eContext));
    actual_encoded = eCtx.get();
    ASSERT_EQ(expected_encoded, actual_encoded);
}

void assert_decode(std::vector<uint8_t> document, std::vector<std::shared_ptr<Event>> expected_events)
{
    if(REPORT_ENCODING || REPORT_DECODING)
    {
        printf("\n[assert_decode]\n");
    }

    DecoderContext dCtx;
    size_t decodedOffset = 0;
    ASSERT_EQ(KSBONJSON_DECODE_OK, ksbonjson_decode(document.data(), document.size(), &dCtx.callbacks, &dCtx, &decodedOffset));
    ASSERT_EQ(document.size(), decodedOffset);
    assert_events_equal(expected_events, dCtx.events);
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

    DecoderContext dCtx;
    size_t decodedOffset = 0;
    ASSERT_NE(KSBONJSON_ENCODE_OK, ksbonjson_decode(document.data(), document.size(), &dCtx.callbacks, &dCtx, &decodedOffset));
}

// ============================================================================
// Tests
// ============================================================================

enum
{
    INTSMALL_BIAS = 117,
    INTSMALL_MAX = 234,
    TYPE_ARRAY = 0xeb,
    TYPE_OBJECT = 0xec,
    TYPE_END = 0xed,
    TYPE_FALSE = 0xee,
    TYPE_TRUE = 0xef,
    TYPE_NULL = 0xf0,
    TYPE_INT8 = 0xf1,
    TYPE_INT16 = 0xf2,
    TYPE_INT24 = 0xf3,
    TYPE_INT32 = 0xf4,
    TYPE_INT40 = 0xf5,
    TYPE_INT48 = 0xf6,
    TYPE_INT56 = 0xf7,
    TYPE_INT64 = 0xf8,
    TYPE_UINT64 = 0xf9,
    TYPE_BIGPOSITIVE = 0xfa,
    TYPE_BIGNEGATIVE = 0xfb,
    TYPE_FLOAT16 = 0xfc,
    TYPE_FLOAT32 = 0xfd,
    TYPE_FLOAT64 = 0xfe,
    TYPE_STRING = 0xff,
};

#define SMALL(VALUE) ( (uint8_t)((VALUE) + INTSMALL_BIAS) )

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
    assert_encode_decode({std::make_shared<IntegerEvent>( 117)}, { 117 + 117});
    assert_encode_decode({std::make_shared<IntegerEvent>( 100)}, { 100 + 117});
    assert_encode_decode({std::make_shared<IntegerEvent>(  10)}, {  10 + 117});
    assert_encode_decode({std::make_shared<IntegerEvent>(   0)}, {   0 + 117});
    assert_encode_decode({std::make_shared<IntegerEvent>(   1)}, {   1 + 117});
    assert_encode_decode({std::make_shared<IntegerEvent>(  -1)}, {  -1 + 117});
    assert_encode_decode({std::make_shared<IntegerEvent>( -60)}, { -60 + 117});
    assert_encode_decode({std::make_shared<IntegerEvent>(-100)}, {-100 + 117});
    assert_encode_decode({std::make_shared<IntegerEvent>(-117)}, {-117 + 117});

    assert_encode_decode({std::make_shared<IntegerEvent>(-117)}, {0x00});

}

TEST(EncodeDecode, int8)
{
    assert_encode_decode({std::make_shared<IntegerEvent>( 245)}, {TYPE_INT8, (uint8_t)( 127)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 244)}, {TYPE_INT8, (uint8_t)( 126)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 240)}, {TYPE_INT8, (uint8_t)( 122)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 220)}, {TYPE_INT8, (uint8_t)( 102)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 120)}, {TYPE_INT8, (uint8_t)(   2)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 119)}, {TYPE_INT8, (uint8_t)(   1)});
    assert_encode_decode({std::make_shared<IntegerEvent>( 118)}, {TYPE_INT8, (uint8_t)(   0)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-118)}, {TYPE_INT8, (uint8_t)(  -1)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-218)}, {TYPE_INT8, (uint8_t)(-101)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-238)}, {TYPE_INT8, (uint8_t)(-121)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-240)}, {TYPE_INT8, (uint8_t)(-123)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-244)}, {TYPE_INT8, (uint8_t)(-127)});
    assert_encode_decode({std::make_shared<IntegerEvent>(-245)}, {TYPE_INT8, (uint8_t)(-128)});
}

TEST(EncodeDecode, int16)
{
    assert_encode_decode({std::make_shared<IntegerEvent>( 246)}, {TYPE_INT16, 246, 0});
    assert_encode_decode({std::make_shared<IntegerEvent>(-246)}, {TYPE_INT16, (uint8_t)(-246), 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(-247)}, {TYPE_INT16, (uint8_t)(-247), 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(1000LL)}, {TYPE_INT16, 0xe8, 0x03});
    assert_encode_decode({std::make_shared<IntegerEvent>(-1000LL)}, {TYPE_INT16, 0x18, 0xfc});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffLL)}, {TYPE_INT16, 0xff, 0x7f});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000LL)}, {TYPE_INT16, 0x00, 0x80});
}

TEST(EncodeDecode, int24)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8000LL)}, {TYPE_INT24, 0x00, 0x80, 0x00});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8001LL)}, {TYPE_INT24, 0xff, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffffLL)}, {TYPE_INT24, 0xff, 0xff, 0x7f});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x800000LL)}, {TYPE_INT24, 0x00, 0x00, 0x80});
}

TEST(EncodeDecode, int32)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0x800000LL)}, {TYPE_INT32, 0x00, 0x00, 0x80, 0x00});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x800001LL)}, {TYPE_INT32, 0xff, 0xff, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffffffLL)}, {TYPE_INT32, 0xff, 0xff, 0xff, 0x7f});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000000LL)}, {TYPE_INT32, 0x00, 0x00, 0x00, 0x80});
}

TEST(EncodeDecode, int40)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0x80000000LL)}, {TYPE_INT40, 0x00, 0x00, 0x00, 0x80, 0x00});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000001LL)}, {TYPE_INT40, 0xff, 0xff, 0xff, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffffffffLL)}, {TYPE_INT40, 0xff, 0xff, 0xff, 0xff, 0x7f});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000000000LL)}, {TYPE_INT40, 0x00, 0x00, 0x00, 0x00, 0x80});
}

TEST(EncodeDecode, int48)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8000000000LL)}, {TYPE_INT48, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000000001LL)}, {TYPE_INT48, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffffffffffLL)}, {TYPE_INT48, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x800000000000LL)}, {TYPE_INT48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
}

TEST(EncodeDecode, int56)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0x800000000000LL)}, {TYPE_INT56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x800000000001LL)}, {TYPE_INT56, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffffffffffffLL)}, {TYPE_INT56, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000000000000LL)}, {TYPE_INT56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
}

TEST(EncodeDecode, int64)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0x80000000000000LL)}, {TYPE_INT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000000000001LL)}, {TYPE_INT64, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffffffffffffffLL)}, {TYPE_INT64, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000000000000000LL)}, {TYPE_INT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
}

TEST(EncodeDecode, uint64)
{
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x8000000000000000ULL)}, {TYPE_UINT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x8000000000000001ULL)}, {TYPE_UINT64, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xffffffffffffffffULL)}, {TYPE_UINT64, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
}

TEST(EncodeDecode, string)
{
    assert_encode_decode({std::make_shared<StringEvent>("")}, {TYPE_STRING, TYPE_STRING});
    assert_encode_decode({std::make_shared<StringEvent>("1")}, {TYPE_STRING, 0x31, TYPE_STRING});
    assert_encode_decode({std::make_shared<StringEvent>("12")}, {TYPE_STRING, 0x31, 0x32, TYPE_STRING});
    assert_encode_decode({std::make_shared<StringEvent>("1234567890123456789012345678901")},
        {
            TYPE_STRING,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31,
            TYPE_STRING,
        });

    assert_encode_decode({std::make_shared<StringEvent>("12345678901234567890123456789012")},
        {
            TYPE_STRING,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
                0x31, 0x32,
            TYPE_STRING,
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
            TYPE_STRING,
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
            SMALL(1),
            TYPE_STRING, 0x78, TYPE_STRING,
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
            TYPE_STRING, 0x31, TYPE_STRING, SMALL(1),
            TYPE_STRING, 0x32, TYPE_STRING, TYPE_STRING, 0x78, TYPE_STRING,
            TYPE_STRING, 0x33, TYPE_STRING, TYPE_NULL,
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
            TYPE_STRING, 0x61, TYPE_STRING,
            SMALL(1),
        TYPE_END,
    });

    // assert_encode_decode(
    // {
    //     std::make_shared<ObjectBeginEvent>(),
    //         std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
    //         std::make_shared<StringChunkEvent>("bc", CHUNK_HAS_NEXT),
    //         std::make_shared<StringChunkEvent>("d", CHUNK_LAST),
    //         std::make_shared<IntegerEvent>(1LL),
    //     std::make_shared<ContainerEndEvent>(),
    // },
    // {
    //     0x92,
    //         0x90, 0x03, 0x61, 0x05, 0x62, 0x63, 0x02, 0x64,
    //         0x01,
    //     0x93,
    // });

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
            TYPE_STRING, 0x61, TYPE_STRING,
            SMALL(1),
            TYPE_STRING, 0x7a, TYPE_STRING,
            SMALL(1),
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
            TYPE_STRING, 0x61, TYPE_STRING,
            SMALL(-1),
            TYPE_STRING, 0x7a, TYPE_STRING,
            SMALL(1),
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_INT16, 0xe8, 0x03,
            TYPE_STRING, 0x7a, TYPE_STRING,
            SMALL(1),
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_INT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
            TYPE_STRING, 0x7a, TYPE_STRING,
            SMALL(1),
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_INT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0,
            TYPE_STRING, 0x7a, TYPE_STRING,
            SMALL(1),
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_FLOAT16, 0xa0, 0x3f,
            TYPE_STRING, 0x7a, TYPE_STRING,
            SMALL(1),
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_FLOAT64, 0x35, 0x3c, 0xce, 0x81, 0x87, 0x29, 0xb6, 0xb5,
            TYPE_STRING, 0x7a, TYPE_STRING,
            SMALL(1),
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_STRING, 0x62, TYPE_STRING,
            TYPE_STRING, 0x7a, TYPE_STRING,
            SMALL(1),
        TYPE_END,
    });

    // assert_encode_decode(
    // {
    //     std::make_shared<ObjectBeginEvent>(),
    //         std::make_shared<StringEvent>("a"),
    //         std::make_shared<StringChunkEvent>("b", CHUNK_HAS_NEXT),
    //         std::make_shared<StringChunkEvent>("c", CHUNK_HAS_NEXT),
    //         std::make_shared<StringChunkEvent>("d", CHUNK_HAS_NEXT),
    //         std::make_shared<StringChunkEvent>("e", CHUNK_LAST),
    //         std::make_shared<StringEvent>("z"),
    //         std::make_shared<IntegerEvent>(1LL),
    //     std::make_shared<ContainerEndEvent>(),
    // },
    // {
    //     TYPE_OBJECT,
    //         0x71, 0x61,
    //         0x90, 0x03, 0x62, 0x03, 0x63, 0x03, 0x64, 0x02, 0x65,
    //         0x71, 0x7a,
    //         0x01,
    //     TYPE_END,
    // });

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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_FALSE,
            TYPE_STRING, 0x7a, TYPE_STRING,
            SMALL(1),
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_NULL,
            TYPE_STRING, 0x7a, TYPE_STRING,
            SMALL(1),
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_OBJECT,
            TYPE_END,
            TYPE_STRING, 0x7a, TYPE_STRING,
            SMALL(1),
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_ARRAY,
            TYPE_END,
            TYPE_STRING, 0x7a, TYPE_STRING,
            SMALL(1),
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
            TYPE_STRING, 0x61, TYPE_STRING,
            SMALL(1),
            TYPE_STRING, 0x7a, TYPE_STRING,
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
            TYPE_STRING, 0x61, TYPE_STRING,
            SMALL(-1),
            TYPE_STRING, 0x7a, TYPE_STRING,
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_INT16, 0xe8, 0x03,
            TYPE_STRING, 0x7a, TYPE_STRING,
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_INT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
            TYPE_STRING, 0x7a, TYPE_STRING,
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_INT64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0,
            TYPE_STRING, 0x7a, TYPE_STRING,
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_FLOAT16, 0xa0, 0x3f,
            TYPE_STRING, 0x7a, TYPE_STRING,
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_FLOAT64, 0x35, 0x3c, 0xce, 0x81, 0x87, 0x29, 0xb6, 0xb5,
            TYPE_STRING, 0x7a, TYPE_STRING,
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_STRING, 0x62, TYPE_STRING,
            TYPE_STRING, 0x7a, TYPE_STRING,
        TYPE_END,
    });

    // assert_encode_decode(
    // {
    //     std::make_shared<ArrayBeginEvent>(),
    //         std::make_shared<StringEvent>("a"),
    //         std::make_shared<StringChunkEvent>("b", CHUNK_HAS_NEXT),
    //         std::make_shared<StringChunkEvent>("c", CHUNK_HAS_NEXT),
    //         std::make_shared<StringChunkEvent>("d", CHUNK_HAS_NEXT),
    //         std::make_shared<StringChunkEvent>("e", CHUNK_LAST),
    //         std::make_shared<StringEvent>("z"),
    //         std::make_shared<IntegerEvent>(1LL),
    //     std::make_shared<ContainerEndEvent>(),
    // },
    // {
    //     TYPE_ARRAY,
    //         0x71, 0x61,
    //         0x90, 0x03, 0x62, 0x03, 0x63, 0x03, 0x64, 0x02, 0x65,
    //         0x71, 0x7a,
    //         0x01,
    //     TYPE_END,
    // });

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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_FALSE,
            TYPE_STRING, 0x7a, TYPE_STRING,
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_NULL,
            TYPE_STRING, 0x7a, TYPE_STRING,
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_OBJECT,
            TYPE_END,
            TYPE_STRING, 0x7a, TYPE_STRING,
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
            TYPE_STRING, 0x61, TYPE_STRING,
            TYPE_ARRAY,
            TYPE_END,
            TYPE_STRING, 0x7a, TYPE_STRING,
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

TEST(Encoder, string_chunking)
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

TEST(Encoder, containers)
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

TEST(Decoder, big_number)
{
    assert_decode({TYPE_BIGPOSITIVE, 0x01, 0x00}, {std::make_shared<UIntegerEvent>(0LL)});
}

TEST(Decoder, big_number_length_field)
{
    assert_decode_failure({TYPE_BIGPOSITIVE});
    assert_decode_failure({TYPE_BIGPOSITIVE, 0x01});
    assert_decode_failure({TYPE_BIGPOSITIVE, 0x01});
    // assert_decode_failure({0x96, 0x01, 0x00});
}


// ------------------------------------
// Example Tests
// ------------------------------------

TEST(Example, specification)
{
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
        0xec,
            0xff, 0x61, 0x20, 0x6e, 0x75, 0x6d, 0x62, 0x65, 0x72, 0xff,
            0x76,
            0xff, 0x61, 0x6e, 0x20, 0x61, 0x72, 0x72, 0x61, 0x79, 0xff,
            0xeb,
                0xff, 0x78, 0xff,
                0xf2, 0xe8, 0x03,
                0xfc, 0xc0, 0x3f,
            0xed,
            0xff, 0x61, 0x20, 0x6e, 0x75, 0x6c, 0x6c, 0xff,
            0xf0,
            0xff, 0x61, 0x20, 0x62, 0x6f, 0x6f, 0x6c, 0x65, 0x61, 0x6e, 0xff,
            0xef,
            0xff, 0x61, 0x6e, 0x20, 0x6f, 0x62, 0x6a, 0x65, 0x63, 0x74, 0xff,
            0xec,
                0xff, 0x61, 0xff,
                0x11,
                0xff, 0x62, 0xff,
                0xff, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
                            0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
                            0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
                            0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
                            0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0xff,
            0xed,
        0xed
    });
}
