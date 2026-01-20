#pragma once

#include <ksbonjson/KSBONJSONEncoder.h>
#include <string>
#include <memory>
#include <vector>
#include <sstream>

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
        return ksbonjson_addSignedInteger(ctx, value);
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
        return ksbonjson_addUnsignedInteger(ctx, value);
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

std::ostream &operator<<(std::ostream &os, KSBigNumber const &v) {
    return os << (v.significandSign < 0 ? "-" : "")
    << v.significand << "e" << v.exponent;
}

bool operator==(KSBigNumber const &l, KSBigNumber const &r) {
    // Just do a naive comparison
    return l.significandSign == r.significandSign &&
    l.significand == r.significand &&
    l.exponent == r.exponent;
}

class BigNumberEvent: public Event
{
public:
    BigNumberEvent(KSBigNumber value)
    : value(value)
    {}
    virtual ~BigNumberEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_addBigNumber(ctx, value);
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
        str << "BIG(" << value << ")";
        return str.str();
    }
private:
    const KSBigNumber value;
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const BigNumberEvent&>(obj);
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
    ObjectBeginEvent(size_t pairCount = 0, bool moreChunksFollow = false)
    : pairCount(pairCount), moreChunksFollow(moreChunksFollow)
    {}
    virtual ~ObjectBeginEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_beginObject(ctx, pairCount, moreChunksFollow);
    }
    virtual std::string comparator() const override
    {
        return "obj";
    }
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "O(" << pairCount << (moreChunksFollow ? ",+" : "") << ")";
        return str.str();
    }
private:
    size_t pairCount;
    bool moreChunksFollow;
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const ObjectBeginEvent&>(obj);
        return Event::isEqual(v);
    }
};

class ArrayBeginEvent: public Event
{
public:
    ArrayBeginEvent(size_t elementCount = 0, bool moreChunksFollow = false)
    : elementCount(elementCount), moreChunksFollow(moreChunksFollow)
    {}
    virtual ~ArrayBeginEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* ctx) override
    {
        return ksbonjson_beginArray(ctx, elementCount, moreChunksFollow);
    }
    virtual std::string comparator() const override
    {
        return "arr";
    }
    virtual std::string description() const override
    {
        std::ostringstream str;
        str << "A(" << elementCount << (moreChunksFollow ? ",+" : "") << ")";
        return str.str();
    }
private:
    size_t elementCount;
    bool moreChunksFollow;
protected:
    virtual bool isEqual(const Event& obj) const override {
        auto v = static_cast<const ArrayBeginEvent&>(obj);
        return Event::isEqual(v);
    }
};

// ContainerEndEvent is used for decoding only - containers close automatically
// during encoding when element counts are exhausted
class ContainerEndEvent: public Event
{
public:
    virtual ~ContainerEndEvent() {}
    virtual ksbonjson_encodeStatus operator()(KSBONJSONEncodeContext* /*ctx*/) override
    {
        // No-op for encoding - containers close automatically
        return KSBONJSON_ENCODE_OK;
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
