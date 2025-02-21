#pragma once

#include <ksbonjson/KSBONJSONDecoder.h>
#include <string>
#include <vector>


namespace ksbonjson
{

class Decoder
{
public:
    Decoder();
    virtual ~Decoder() {}
    virtual ksbonjson_decodeStatus decode(const uint8_t* KSBONJSON_RESTRICT document,
                                          size_t documentLength,
                                          size_t* KSBONJSON_RESTRICT decodedOffset)
    {
        return ::ksbonjson_decode(document, documentLength, &callbacks, this, decodedOffset);
    }
    virtual ksbonjson_decodeStatus decode(std::vector<uint8_t> document,
                                          size_t* decodedOffset)
    {
        return decode(document.data(), document.size(), decodedOffset);
    }

protected:
    virtual ksbonjson_decodeStatus onValue(bool value) = 0;
    virtual ksbonjson_decodeStatus onValue(int64_t value) = 0;
    virtual ksbonjson_decodeStatus onValue(uint64_t value) = 0;
    virtual ksbonjson_decodeStatus onValue(double value) = 0;
    virtual ksbonjson_decodeStatus onValue(KSBigNumber value) = 0;
    virtual ksbonjson_decodeStatus onString(const char* value, size_t length) = 0;
    virtual ksbonjson_decodeStatus onNull() = 0;
    virtual ksbonjson_decodeStatus onBeginObject() = 0;
    virtual ksbonjson_decodeStatus onBeginArray() = 0;
    virtual ksbonjson_decodeStatus onEndContainer() = 0;
    virtual ksbonjson_decodeStatus onEndData() = 0;

private:
    KSBONJSONDecodeCallbacks callbacks;

    friend ksbonjson_decodeStatus onValue(bool value, void* userData);
    friend ksbonjson_decodeStatus onValue(int64_t value, void* userData);
    friend ksbonjson_decodeStatus onValue(uint64_t value, void* userData);
    friend ksbonjson_decodeStatus onValue(double value, void* userData);
    friend ksbonjson_decodeStatus onValue(KSBigNumber value, void* userData);
    friend ksbonjson_decodeStatus onString(const char* KSBONJSON_RESTRICT value,
                                           size_t length,
                                           void* KSBONJSON_RESTRICT userData);
    friend ksbonjson_decodeStatus onNull(void* userData);
    friend ksbonjson_decodeStatus onBeginObject(void* userData);
    friend ksbonjson_decodeStatus onBeginArray(void* userData);
    friend ksbonjson_decodeStatus onEndContainer(void* userData);
    friend ksbonjson_decodeStatus onEndData(void* userData);
};

ksbonjson_decodeStatus onValue(bool value, void* userData)
{
    return ((Decoder*)userData)->onValue(value);
}
ksbonjson_decodeStatus onValue(int64_t value, void* userData)
{
    return ((Decoder*)userData)->onValue(value);
}
ksbonjson_decodeStatus onValue(uint64_t value, void* userData)
{
    return ((Decoder*)userData)->onValue(value);
}
ksbonjson_decodeStatus onValue(double value, void* userData)
{
    return ((Decoder*)userData)->onValue(value);
}
ksbonjson_decodeStatus onValue(KSBigNumber value, void* userData)
{
    return ((Decoder*)userData)->onValue(value);
}
ksbonjson_decodeStatus onNull(void* userData)
{
    return ((Decoder*)userData)->onNull();
}
ksbonjson_decodeStatus onString(const char* KSBONJSON_RESTRICT value,
                                size_t length,
                                void* KSBONJSON_RESTRICT userData)
{
    return ((Decoder*)userData)->onString(value, length);
}
ksbonjson_decodeStatus onBeginObject(void* userData)
{
    return ((Decoder*)userData)->onBeginObject();
}
ksbonjson_decodeStatus onBeginArray(void* userData)
{
    return ((Decoder*)userData)->onBeginArray();
}
ksbonjson_decodeStatus onEndContainer(void* userData)
{
    return ((Decoder*)userData)->onEndContainer();
}
ksbonjson_decodeStatus onEndData(void* userData)
{
    return ((Decoder*)userData)->onEndData();
}

Decoder::Decoder()
: callbacks{
    .onBoolean = ksbonjson::onValue,
    .onUnsignedInteger = ksbonjson::onValue,
    .onSignedInteger = ksbonjson::onValue,
    .onFloat = ksbonjson::onValue,
    .onBigNumber = ksbonjson::onValue,
    .onNull = ksbonjson::onNull,
    .onString = ksbonjson::onString,
    .onBeginObject = ksbonjson::onBeginObject,
    .onBeginArray = ksbonjson::onBeginArray,
    .onEndContainer = ksbonjson::onEndContainer,
    .onEndData = ksbonjson::onEndData,
}
{}


}
