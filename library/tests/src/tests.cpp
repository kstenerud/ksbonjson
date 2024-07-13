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

// ============================================================================
// Events
// ============================================================================

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

class NameEvent: public Event
{
public:
    NameEvent(std::string name)
    : name(name)
    {}
    NameEvent(const char* name)
    : name(name)
    {}
    NameEvent(const char* name, size_t length)
    : name(name, length)
    {}
    virtual ~NameEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_addName(ctx, name.c_str(), name.length());
    }
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "Name(" << name << ")";
        return str.str();
    }
private:
    std::string name;
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const NameEvent&>(obj);
        return Event::isEqual(v) && v.name == name;
    }
};

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
    StringChunkEvent(std::string value, bool isLastChunk)
    : value(value)
    , isLastChunk(isLastChunk)
    {}
    StringChunkEvent(const char* value, bool isLastChunk)
    : value(value)
    , isLastChunk(isLastChunk)
    {}
    StringChunkEvent(const char* value, size_t length, bool isLastChunk)
    : value(value, length)
    , isLastChunk(isLastChunk)
    {}
    virtual ~StringChunkEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_chunkString(ctx, value.c_str(), value.length(), isLastChunk);
    }
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "Chunk(" << value << ")";
        return str.str();
    }
private:
    std::string value;
    bool isLastChunk;
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const StringChunkEvent&>(obj);
        return Event::isEqual(v) && (v.value == value) && (v.isLastChunk = isLastChunk);
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

static ksbonjson_decodeStatus onName(const char* KSBONJSON_RESTRICT name,
                                     size_t nameLength,
                                     void* KSBONJSON_RESTRICT userData)
{
    DecoderContext* ctx = (DecoderContext*)userData;
    ctx->addEvent(std::make_shared<NameEvent>(name, nameLength));
    return KSBONJSON_DECODE_OK;
}

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
    ctx->addEvent(std::make_shared<StringChunkEvent>(value, length, isLastChunk));
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
    .onName = onName,
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


// ============================================================================
// Test Support
// ============================================================================

void assert_success(std::vector<std::shared_ptr<Event>> events, std::vector<uint8_t> expected_encoded)
{
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
    DecoderContext dCtx;
    size_t decodedOffset = 0;
    ASSERT_NE(KSBONJSON_ENCODE_OK, ksbonjson_decode(document.data(), document.size(), &dCtx.callbacks, &dCtx, &decodedOffset));
}

// ============================================================================
// Tests
// ============================================================================

TEST(Encoder, bad_stuff)
{
    assert_encode_failure(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<IntegerEvent>(1),
            std::make_shared<IntegerEvent>(1),
        std::make_shared<ContainerEndEvent>(),
    });
}

TEST(Decoder, bad_stuff)
{
    assert_decode_failure({0x92, 0x01, 0x01, 0x93});
}

TEST(Encoder, example)
{
    assert_success(
    {
        std::make_shared<ObjectBeginEvent>(),
            std::make_shared<NameEvent>("a number"),
            std::make_shared<IntegerEvent>(1),
            std::make_shared<NameEvent>("an array"),
            std::make_shared<ArrayBeginEvent>(),
                std::make_shared<StringEvent>("x"),
                std::make_shared<IntegerEvent>(1000),
                // std::make_shared<FloatEvent>(1.5), TODO
            std::make_shared<ContainerEndEvent>(),
            std::make_shared<NameEvent>("a null"),
            std::make_shared<NullEvent>(),
            std::make_shared<NameEvent>("a boolean"),
            std::make_shared<BooleanEvent>(true),
            std::make_shared<NameEvent>("an object"),
            std::make_shared<ObjectBeginEvent>(),
                std::make_shared<NameEvent>("a"),
                std::make_shared<IntegerEvent>(-100),
                std::make_shared<NameEvent>("b"),
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
                // 0x6f, 0x05, 0x0f, 0x01,
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
