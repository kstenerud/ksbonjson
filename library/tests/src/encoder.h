#pragma once

#include <ksbonjson/KSBONJSONEncoder.h>
#include <string>

namespace ksbonjson
{

class Encoder
{
protected:
    virtual ksbonjson_encodeStatus addEncodedData(const uint8_t* data, size_t length) = 0;

public:
    Encoder() {}
    virtual ~Encoder() {}

    void begin();

    ksbonjson_encodeStatus end()
    {
        return ksbonjson_endEncode(&context);
    }

    ksbonjson_encodeStatus terminate()
    {
        return ksbonjson_terminateDocument(&context);
    }

    ksbonjson_encodeStatus addValue(bool value)
    {
        return ksbonjson_addBoolean(&context, value);
    }

    ksbonjson_encodeStatus addValue(int64_t value)
    {
        return ksbonjson_addSignedInteger(&context, value);
    }

    ksbonjson_encodeStatus addValue(uint64_t value)
    {
        return ksbonjson_addUnsignedInteger(&context, value);
    }

    ksbonjson_encodeStatus addValue(double value)
    {
        return ksbonjson_addFloat(&context, value);
    }

    ksbonjson_encodeStatus addValue(std::string value)
    {
        return addString(value.c_str(), value.length());
    }

    ksbonjson_encodeStatus addString(const char* value, size_t length)
    {
        return ksbonjson_addString(&context, value, length);
    }

    ksbonjson_encodeStatus addNull()
    {
        return ksbonjson_addNull(&context);
    }

    ksbonjson_encodeStatus beginObject()
    {
        return ksbonjson_beginObject(&context);
    }

    ksbonjson_encodeStatus beginArray()
    {
        return ksbonjson_beginArray(&context);
    }

    ksbonjson_encodeStatus endContainer()
    {
        return ksbonjson_endContainer(&context);
    }

private:
    KSBONJSONEncodeContext context;

    friend ksbonjson_encodeStatus addEncodedData(const uint8_t* KSBONJSON_RESTRICT data,
                                                size_t length,
                                                void* KSBONJSON_RESTRICT userData);
};

ksbonjson_encodeStatus addEncodedData(const uint8_t* KSBONJSON_RESTRICT data,
                                      size_t length,
                                      void* KSBONJSON_RESTRICT userData)
{
    return ((Encoder*)userData)->addEncodedData(data, length);
}

void Encoder::begin()
{
    ksbonjson_beginEncode(&context, ksbonjson::addEncodedData, this);
}

}
