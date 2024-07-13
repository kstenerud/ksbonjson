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
    virtual ~Event() {}
protected:
    friend bool operator==(const Event&, const Event&);
    virtual bool isEqual(const Event&) const {return true;};
};

bool operator==(const Event& lhs, const Event& rhs) {
    return typeid(lhs) == typeid(rhs) && lhs.isEqual(rhs);
}
std::ostream &operator<<(std::ostream &os, Event const &e) { 
    return os << e.description();
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
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "Boolean(" << (value ? "true" : "false") << ")";
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
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "Integer(" << value << ")";
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
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "UInteger(" << value << ")";
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
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "Float(" << value << ")";
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
    virtual std::string description() const override
    {
        return "Null()";
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
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "String(" << value << ")";
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
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "Chunk(" << value << ")";
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
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "BONJSON(" << value.size() << ")";
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
    virtual std::string description() const override
    {
        return "Object()";
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
    virtual std::string description() const override
    {
        return "Array()";
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
    virtual std::string description() const override
    {
        return "EndContainer()";
    }
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const ContainerEndEvent&>(obj);
        return Event::isEqual(v);
    }
};

class DataEndEvent: public Event
{
public:
    virtual ~DataEndEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_endContainer(ctx);
    }
    virtual std::string description() const override
    {
        return "EndData()";
    }
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const DataEndEvent&>(obj);
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

static ksbonjson_decodeStatus onStringChunk(const char* KSBONJSON_RESTRICT value,
                                            size_t length,
                                            bool isLastChunk,
                                            void* KSBONJSON_RESTRICT userData)
{
    DecoderContext* ctx = (DecoderContext*)userData;
    ctx->addEvent(std::make_shared<StringChunkEvent>(value, length, isLastChunk ? CHUNK_LAST : CHUNK_HAS_NEXT));
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
    DecoderContext* ctx = (DecoderContext*)userData;
    ctx->addEvent(std::make_shared<DataEndEvent>());
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
    .onStringChunk = onStringChunk,
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

void assert_encode_decode(std::vector<std::shared_ptr<Event>> events, std::vector<uint8_t> expected_encoded)
{
    if(REPORT_ENCODING || REPORT_DECODING)
    {
        printf("\n[assert_encode_decode]\n");
    }

    // Encode
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
    DecoderContext dCtx;
    size_t decodedOffset = 0;
    std::vector<uint8_t> document = actual_encoded;
    ASSERT_EQ(KSBONJSON_DECODE_OK, ksbonjson_decode(document.data(), document.size(), &dCtx.callbacks, &dCtx, &decodedOffset));
    ASSERT_EQ(expected_encoded.size(), decodedOffset);

    // Encode again
    eCtx.reset();
    memset(&eContext, 0, sizeof(eContext));
    ksbonjson_beginEncode(&eContext, addEncodedDataCallback, &eCtx);
    for (const std::shared_ptr<Event>& event: events)
    {
        ASSERT_EQ(KSBONJSON_ENCODE_OK, (*event)(&eContext));
    }
    ASSERT_EQ(KSBONJSON_ENCODE_OK, ksbonjson_endEncode(&eContext));
    actual_encoded = eCtx.get();
    ASSERT_EQ(expected_encoded, actual_encoded);
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


// ------------------------------------
// Basic Tests
// ------------------------------------

TEST(EncodeDecode, null)
{
    assert_encode_decode({std::make_shared<NullEvent>()}, {0x6a});
}

TEST(EncodeDecode, boolean)
{
    assert_encode_decode({std::make_shared<BooleanEvent>(true)}, {0x95});
    assert_encode_decode({std::make_shared<BooleanEvent>(false)}, {0x94});
}

TEST(EncodeDecode, smallint)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0)}, {0x00});
    assert_encode_decode({std::make_shared<IntegerEvent>(1)}, {0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(-1)}, {0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(10)}, {0x0a});
    assert_encode_decode({std::make_shared<IntegerEvent>(-60)}, {0xc4});
    assert_encode_decode({std::make_shared<IntegerEvent>(105)}, {0x69});
    assert_encode_decode({std::make_shared<IntegerEvent>(-106)}, {0x96});
}

TEST(EncodeDecode, int16)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(106)}, {0x6b, 0x6a, 0x00});
    assert_encode_decode({std::make_shared<IntegerEvent>(-107)}, {0x6b, 0x95, 0xff});
    assert_encode_decode({std::make_shared<IntegerEvent>(1000)}, {0x6b, 0xe8, 0x03});
    assert_encode_decode({std::make_shared<IntegerEvent>(-1000)}, {0x6b, 0x18, 0xfc});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fff)}, {0x6b, 0xff, 0x7f});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000)}, {0x6b, 0x00, 0x80});
}

TEST(EncodeDecode, float32)
{
    assert_encode_decode({std::make_shared<FloatEvent>(1.125)}, {0x6c, 0x00, 0x00, 0x90, 0x3f});
}

TEST(EncodeDecode, float64)
{
    assert_encode_decode({std::make_shared<FloatEvent>(1.234)}, {0x6d, 0x58, 0x39, 0xb4, 0xc8, 0x76, 0xbe, 0xf3, 0x3f});
}

TEST(EncodeDecode, bigint)
{
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8000LL)}, {0x6e, 0x08, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x9876LL)}, {0x6e, 0x08, 0x76, 0x98});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xffffLL)}, {0x6e, 0x08, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8001LL)}, {0x6f, 0x08, 0x01, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xcba9LL)}, {0x6f, 0x08, 0xa9, 0xcb});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xffffLL)}, {0x6f, 0x08, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(0x10000LL)}, {0x6e, 0x0c, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x800000LL)}, {0x6e, 0x0c, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xffffffLL)}, {0x6e, 0x0c, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(-0x10000LL)}, {0x6f, 0x0c, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x89abcdLL)}, {0x6f, 0x0c, 0xcd, 0xab, 0x89});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xffffffLL)}, {0x6f, 0x0c, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(0x1000000LL)}, {0x6e, 0x10, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x80000000LL)}, {0x6e, 0x10, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xffffffffLL)}, {0x6e, 0x10, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(-0x1000000LL)}, {0x6f, 0x10, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000001LL)}, {0x6f, 0x10, 0x01, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xffffffffLL)}, {0x6f, 0x10, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(0x100000000LL)}, {0x6e, 0x14, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x8000000000LL)}, {0x6e, 0x14, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xffffffffffLL)}, {0x6e, 0x14, 0xff, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(-0x100000000LL)}, {0x6f, 0x14, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000000001LL)}, {0x6f, 0x14, 0x01, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xffffffffffLL)}, {0x6f, 0x14, 0xff, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(0x10000000000LL)}, {0x6e, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x800000000000LL)}, {0x6e, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xffffffffffffLL)}, {0x6e, 0x18, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(-0x10000000000LL)}, {0x6f, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x800000000001LL)}, {0x6f, 0x18, 0x01, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xffffffffffffLL)}, {0x6f, 0x18, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(0x1000000000000LL)}, {0x6e, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x80000000000000LL)}, {0x6e, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(0xffffffffffffffLL)}, {0x6e, 0x1c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(-0x1000000000000LL)}, {0x6f, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x80000000000001LL)}, {0x6f, 0x1c, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0xffffffffffffffLL)}, {0x6f, 0x1c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<IntegerEvent>(0x100000000000000LL)}, {0x6e, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(0x7fffffffffffffffLL)}, {0x6e, 0x20, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});

    assert_encode_decode({std::make_shared<IntegerEvent>(-0x100000000000000LL)}, {0x6f, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<IntegerEvent>(-0x8000000000000000LL)}, {0x6f, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
}

TEST(EncodeDecode, biguint)
{
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x8000ULL)}, {0x6e, 0x08, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x9876ULL)}, {0x6e, 0x08, 0x76, 0x98});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xffffULL)}, {0x6e, 0x08, 0xff, 0xff});

    assert_encode_decode({std::make_shared<UIntegerEvent>(0x10000ULL)}, {0x6e, 0x0c, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x800000ULL)}, {0x6e, 0x0c, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xffffffULL)}, {0x6e, 0x0c, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<UIntegerEvent>(0x1000000ULL)}, {0x6e, 0x10, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x80000000ULL)}, {0x6e, 0x10, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xffffffffULL)}, {0x6e, 0x10, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<UIntegerEvent>(0x100000000ULL)}, {0x6e, 0x14, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x8000000000ULL)}, {0x6e, 0x14, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xffffffffffULL)}, {0x6e, 0x14, 0xff, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<UIntegerEvent>(0x10000000000ULL)}, {0x6e, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x800000000000ULL)}, {0x6e, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xffffffffffffULL)}, {0x6e, 0x18, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<UIntegerEvent>(0x1000000000000ULL)}, {0x6e, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x80000000000000ULL)}, {0x6e, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xffffffffffffffULL)}, {0x6e, 0x1c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});

    assert_encode_decode({std::make_shared<UIntegerEvent>(0x100000000000000ULL)}, {0x6e, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0x7fffffffffffffffULL)}, {0x6e, 0x20, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});
    assert_encode_decode({std::make_shared<UIntegerEvent>(0xffffffffffffffffULL)}, {0x6e, 0x20, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
}

TEST(EncodeDecode, shortstring)
{
    assert_encode_decode({std::make_shared<StringEvent>("")}, {0x70});
    assert_encode_decode({std::make_shared<StringEvent>("1")}, {0x71, 0x31});
    assert_encode_decode({std::make_shared<StringEvent>("12")}, {0x72, 0x31, 0x32});
    assert_encode_decode({std::make_shared<StringEvent>("1234567890123456789012345678901")},
        {0x8f, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
               0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
               0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31,
        });
}

TEST(EncodeDecode, longstring)
{
    assert_encode_decode({std::make_shared<StringEvent>("12345678901234567890123456789012")},
        {0x90, 0x40,
         0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
         0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
         0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
         0x31, 0x32,
        });

    assert_encode_decode({std::make_shared<StringEvent>("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890")},
        {0x90, 0x84, 0x02,
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
    assert_encode_decode({std::make_shared<ArrayBeginEvent>(), std::make_shared<ContainerEndEvent>()}, {0x91, 0x93});
    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<IntegerEvent>(1),
            std::make_shared<StringEvent>("x"),
            std::make_shared<NullEvent>(),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x01,
            0x71, 0x78,
            0x6a,
        0x93
    });
}

TEST(EncodeDecode, object)
{
    assert_encode_decode({std::make_shared<ObjectBeginEvent>(), std::make_shared<ContainerEndEvent>()}, {0x92, 0x93});
    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("1"), std::make_shared<IntegerEvent>(1),
            std::make_shared<StringEvent>("2"), std::make_shared<StringEvent>("x"),
            std::make_shared<StringEvent>("3"), std::make_shared<NullEvent>(),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x31, 0x01,
            0x71, 0x32, 0x71, 0x78,
            0x71, 0x33, 0x6a,
        0x93
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
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringChunkEvent>("a", CHUNK_HAS_NEXT),
            std::make_shared<StringChunkEvent>("bc", CHUNK_HAS_NEXT),
            std::make_shared<StringChunkEvent>("d", CHUNK_LAST),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },

    {
        0x92,
            0x90, 0x03, 0x61, 0x05, 0x62, 0x63, 0x02, 0x64,
            0x01,
        0x93,
    });

    // Non-string is not allowed in the name field

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<IntegerEvent>(1),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<IntegerEvent>(1000),
            std::make_shared<IntegerEvent>(1),
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
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<FloatEvent>(1.234),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<NullEvent>(),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<IntegerEvent>(1),
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
            std::make_shared<IntegerEvent>(1),
            std::make_shared<StringEvent>("z"),
        std::make_shared<ContainerEndEvent>(),
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0x01,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(-1),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0xff,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1000),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0x6b, 0xe8, 0x03,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(0x1000000000000000LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0x6e, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(-0x1000000000000000LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0x6f, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<FloatEvent>(1.25),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0x6c, 0x00, 0x00, 0xa0, 0x3f,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<FloatEvent>(-5.923441e-50),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0x6d, 0x35, 0x3c, 0xce, 0x81, 0x87, 0x29, 0xb6, 0xb5,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<StringEvent>("b"),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0x71, 0x62,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<StringChunkEvent>("b", CHUNK_HAS_NEXT),
            std::make_shared<StringChunkEvent>("c", CHUNK_HAS_NEXT),
            std::make_shared<StringChunkEvent>("d", CHUNK_HAS_NEXT),
            std::make_shared<StringChunkEvent>("e", CHUNK_LAST),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0x90, 0x03, 0x62, 0x03, 0x63, 0x03, 0x64, 0x02, 0x65,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<BooleanEvent>(false),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0x94,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<NullEvent>(),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0x6a,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0x92,
            0x93,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x71, 0x61,
            0x91,
            0x93,
            0x71, 0x7a,
            0x01,
        0x93,
    });

}

TEST(Encoder, array_value)
{
    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x71, 0x61,
            0x01,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(-1),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x71, 0x61,
            0xff,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(1000),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x71, 0x61,
            0x6b, 0xe8, 0x03,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(0x1000000000000000LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x71, 0x61,
            0x6e, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<IntegerEvent>(-0x1000000000000000LL),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x71, 0x61,
            0x6f, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<FloatEvent>(1.25),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x71, 0x61,
            0x6c, 0x00, 0x00, 0xa0, 0x3f,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<FloatEvent>(-5.923441e-50),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x71, 0x61,
            0x6d, 0x35, 0x3c, 0xce, 0x81, 0x87, 0x29, 0xb6, 0xb5,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<StringEvent>("b"),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x71, 0x61,
            0x71, 0x62,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<StringChunkEvent>("b", CHUNK_HAS_NEXT),
            std::make_shared<StringChunkEvent>("c", CHUNK_HAS_NEXT),
            std::make_shared<StringChunkEvent>("d", CHUNK_HAS_NEXT),
            std::make_shared<StringChunkEvent>("e", CHUNK_LAST),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x71, 0x61,
            0x90, 0x03, 0x62, 0x03, 0x63, 0x03, 0x64, 0x02, 0x65,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<BooleanEvent>(false),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x71, 0x61,
            0x94,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<NullEvent>(),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x71, 0x61,
            0x6a,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ObjectBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x71, 0x61,
            0x92,
            0x93,
            0x71, 0x7a,
            0x01,
        0x93,
    });

    assert_encode_decode(
    {
        std::make_shared<ArrayBeginEvent>(),
            std::make_shared<StringEvent>("a"),
            std::make_shared<ArrayBeginEvent>(),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("z"),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x91,
            0x71, 0x61,
            0x91,
            0x93,
            0x71, 0x7a,
            0x01,
        0x93,
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
    ASSERT_NE(KSBONJSON_ENCODE_OK, (*std::make_shared<IntegerEvent>(1))(&eContext));
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
            std::make_shared<IntegerEvent>(1),
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


TEST(Decoder, bad_stuff)
{
    assert_decode_failure({0x92, 0x01, 0x01, 0x93});
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
            std::make_shared<IntegerEvent>(1),
            std::make_shared<StringEvent>("an array"),
            std::make_shared<ArrayBeginEvent>(),
                std::make_shared<StringEvent>("x"),
                std::make_shared<IntegerEvent>(1000),
                std::make_shared<FloatEvent>(1.5),
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<StringEvent>("a null"),
            std::make_shared<NullEvent>(),
            std::make_shared<StringEvent>("a boolean"),
            std::make_shared<BooleanEvent>(true),
            std::make_shared<StringEvent>("an object"),
            std::make_shared<ObjectBeginEvent>(),
                std::make_shared<StringEvent>("a"),
                std::make_shared<IntegerEvent>(-100),
                std::make_shared<StringEvent>("b"),
                std::make_shared<StringEvent>("........................................"),
            std::make_shared<ContainerEndEvent>(),
        std::make_shared<ContainerEndEvent>(),
    },
    {
        0x92,
            0x78, 0x61, 0x20, 0x6e, 0x75, 0x6d, 0x62, 0x65, 0x72,
            0x01,
            0x78, 0x61, 0x6e, 0x20, 0x61, 0x72, 0x72, 0x61, 0x79,
            0x91,
                0x71, 0x78,
                0x6b, 0xe8, 0x03,
                0x6c, 0x00, 0x00, 0xc0, 0x3f,
            0x93,
            0x76, 0x61, 0x20, 0x6e, 0x75, 0x6c, 0x6c,
            0x6a,
            0x79, 0x61, 0x20, 0x62, 0x6f, 0x6f, 0x6c, 0x65, 0x61, 0x6e,
            0x95,
            0x79, 0x61, 0x6e, 0x20, 0x6f, 0x62, 0x6a, 0x65, 0x63, 0x74,
            0x92,
                0x71, 0x61,
                0x9c,
                0x71, 0x62,
                0x90, 0x50, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
                            0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
                            0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
                            0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
                            0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
            0x93,
        0x93
    });
}
