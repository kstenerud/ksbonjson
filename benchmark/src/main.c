//
//  main.c
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
#include <ksbonjson/KSBONJSONDecoder.h>

#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


// Max size of file this program will read before balking.
// Note: The entire file gets loaded into memory, so choose wisely.
#define MAX_FILE_SIZE 5000000000


// ============================================================================
// Utility
// ============================================================================

// Compiler hints for "if" statements
#define likely_if(x) if(__builtin_expect(x,1))
#define unlikely_if(x) if(__builtin_expect(x,0))

#define MARK_UNUSED(x) (void)(x)

static void printErrorArgs(const char* const fmt, va_list args)
{
    vfprintf(stderr, fmt, args);
}

static void printError(const char* const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printErrorArgs(fmt, args);
    va_end(args);
}

static void printErrorAndExit(const char* const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printErrorArgs(fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

static void printPErrorAndExit(const char* const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printErrorArgs(fmt, args);
    va_end(args);
    fprintf(stderr, ": ");
    perror("");
    exit(1);
}

static FILE* openFileForReading(const char* const filename)
{
    if(strcmp(filename, "-") == 0)
    {
        return stdin;
    }
    FILE* const file = fopen(filename, "rb");
    if(file == NULL)
    {
        printPErrorAndExit("Could not open %s", filename);
    }
    return file;
}

static FILE* openFileForWriting(const char* const filename)
{
    if(strcmp(filename, "-") == 0)
    {
        return stdout;
    }
    FILE* const file = fopen(filename, "wb");
    if(file == NULL)
    {
        printPErrorAndExit("Could not open %s", filename);
    }
    return file;
}

static void closeFile(FILE* const file)
{
    if(file != stdin && file != stdout && file != stderr && file != NULL)
    {
        if(fclose(file) == EOF)
        {
            printPErrorAndExit("Could not close file");
        }
    }
}

static uint8_t* readEntireFile(FILE* const file, size_t* const fileLength)
{
    size_t bufferSize = 1024*1024*16;
    uint8_t* buffer = malloc(bufferSize);
    size_t bufferOffset = 0;
    for(;;)
    {
        size_t length = bufferSize - bufferOffset;
        size_t bytes_read = fread(buffer+bufferOffset, 1, length, file);
        if(ferror(file))
        {
            printPErrorAndExit("Could not read %d bytes from file", length);
        }
        bufferOffset += bytes_read;
        if(feof(file))
        {
            break;
        }
        if(bufferOffset > (bufferSize/3)*2)
        {
            if(bufferOffset >= MAX_FILE_SIZE)
            {
                printErrorAndExit("Exceeded max file size of %d", MAX_FILE_SIZE);
            }

            bufferSize *= 2;
            if(bufferSize > MAX_FILE_SIZE)
            {
                bufferSize = MAX_FILE_SIZE;
            }
            buffer = realloc(buffer, bufferSize);
        }
    }
    *fileLength = bufferOffset;
    return buffer;
}

static void writeToFile(FILE* const file, const uint8_t* const data, const int length)
{
    if(fwrite(data, 1, length, file) != (unsigned)length)
    {
        printPErrorAndExit("Could not write %d bytes to file", length);
    }
}


// ============================================================================
// BONJSON to BONJSON
// ============================================================================

typedef struct
{
    KSBONJSONEncodeContext encoderContext;
    uint8_t* buffer;
    size_t size;
    size_t pos;
} bonjson_context;

static bonjson_context* newBonjsonContext(size_t buffer_size)
{
    bonjson_context* const ctx = calloc(1, sizeof(bonjson_context));
    ctx->buffer = calloc(1, buffer_size);
    ctx->size = buffer_size;
    ctx->pos = 0;
    return ctx;
}

static void freeBonjsonContext(bonjson_context* ctx)
{
    free(ctx->buffer);
    free(ctx);
}

static ksbonjson_encodeStatus addEncodedDataCallback(const uint8_t* KSBONJSON_RESTRICT data,
                                              size_t dataLength,
                                              void* KSBONJSON_RESTRICT userData)
{
    bonjson_context* const ctx = (bonjson_context*)userData;
    if(ctx->pos + dataLength > ctx->size)
    {
        printErrorAndExit("BUG: BONJSON buffer was too small (%d + %d > %d)", ctx->pos, dataLength, ctx->size);
    }
    memcpy(ctx->buffer+ctx->pos, data, dataLength);
    ctx->pos += dataLength;

    return KSBONJSON_ENCODE_OK;
}

#define CALL_ENCODER(CALL) \
    do \
    { \
        ksbonjson_encodeStatus result = CALL; \
        if(result != KSBONJSON_ENCODE_OK) \
        { \
            printf(#CALL " failed: %s\n", ksbonjson_describeEncodeStatus(result)); \
            return KSBONJSON_DECODE_COULD_NOT_PROCESS_DATA; \
        } \
        return KSBONJSON_DECODE_OK; \
    } while(0)

static KSBONJSONEncodeContext* getEncoderContext(void* userData)
{
    return &((bonjson_context*)userData)->encoderContext;
}

static ksbonjson_decodeStatus onBoolean(bool value, void* userData)
{
    CALL_ENCODER(ksbonjson_addBoolean(getEncoderContext(userData), value));
}

static ksbonjson_decodeStatus onUnsignedInteger(uint64_t value, void* userData)
{
    CALL_ENCODER(ksbonjson_addUnsignedInteger(getEncoderContext(userData), value));
}

static ksbonjson_decodeStatus onSignedInteger(int64_t value, void* userData)
{
    CALL_ENCODER(ksbonjson_addSignedInteger(getEncoderContext(userData), value));
}

static ksbonjson_decodeStatus onFloat(double value, void* userData)
{
    CALL_ENCODER(ksbonjson_addFloat(getEncoderContext(userData), value));
}

static ksbonjson_decodeStatus onNull(void* userData)
{
    CALL_ENCODER(ksbonjson_addNull(getEncoderContext(userData)));
}

static ksbonjson_decodeStatus onString(const char* KSBONJSON_RESTRICT value,
                                       size_t length,
                                       void* KSBONJSON_RESTRICT userData)
{
    CALL_ENCODER(ksbonjson_addString(getEncoderContext(userData), value, length));
}

static ksbonjson_decodeStatus onBeginObject(size_t elementCountHint, void* userData)
{
    CALL_ENCODER(ksbonjson_beginObject(getEncoderContext(userData), elementCountHint, false));
}

static ksbonjson_decodeStatus onBeginArray(size_t elementCountHint, void* userData)
{
    CALL_ENCODER(ksbonjson_beginArray(getEncoderContext(userData), elementCountHint, false));
}

static ksbonjson_decodeStatus onEndContainer(void* userData)
{
    // Containers auto-close when all elements are added, so nothing to do here
    MARK_UNUSED(userData);
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onEndData(void* userData)
{
    MARK_UNUSED(userData);
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onBooleanDoNothing(bool, void*) {return 0;}
static ksbonjson_decodeStatus onUnsignedIntegerDoNothing(uint64_t, void*) {return 0;}
static ksbonjson_decodeStatus onSignedIntegerDoNothing(int64_t, void*) {return 0;}
static ksbonjson_decodeStatus onFloatDoNothing(double, void*) {return 0;}
static ksbonjson_decodeStatus onNullDoNothing(void*) {return 0;}
static ksbonjson_decodeStatus onStringDoNothing(const char* KSBONJSON_RESTRICT, size_t, void* KSBONJSON_RESTRICT) {return 0;}
static ksbonjson_decodeStatus onBeginObjectDoNothing(size_t, void*) {return 0;}
static ksbonjson_decodeStatus onBeginArrayDoNothing(size_t, void*) {return 0;}
static ksbonjson_decodeStatus onEndContainerDoNothing(void*) {return 0;}
static ksbonjson_decodeStatus onEndDataDoNothing(void*) {return 0;}

static void initBonjsonDecodeCallbacks(KSBONJSONDecodeCallbacks* callbacks, bool decodeOnly)
{
    callbacks->onBeginArray = decodeOnly ?      onBeginArrayDoNothing : onBeginArray;
    callbacks->onBeginObject = decodeOnly ?     onBeginObjectDoNothing : onBeginObject;
    callbacks->onBoolean = decodeOnly ?         onBooleanDoNothing : onBoolean;
    callbacks->onEndContainer = decodeOnly ?    onEndContainerDoNothing : onEndContainer;
    callbacks->onEndData = decodeOnly ?         onEndDataDoNothing : onEndData;
    callbacks->onFloat = decodeOnly ?           onFloatDoNothing : onFloat;
    callbacks->onUnsignedInteger = decodeOnly ? onUnsignedIntegerDoNothing : onUnsignedInteger;
    callbacks->onSignedInteger = decodeOnly ?   onSignedIntegerDoNothing : onSignedInteger;
    callbacks->onNull = decodeOnly ?            onNullDoNothing : onNull;
    callbacks->onString = decodeOnly ?          onStringDoNothing : onString;
}

static void bonjsonToBonjson(const char* const src_path, const char* const dst_path, bool decodeOnly)
{
    FILE* file = openFileForReading(src_path);
    size_t documentSize = 0;
    const uint8_t* document = readEntireFile(file, &documentSize);
    closeFile(file);
    size_t decodedOffset;

    bonjson_context* ctx = newBonjsonContext(documentSize*2);
    ksbonjson_beginEncode(&ctx->encoderContext, addEncodedDataCallback, ctx);

    KSBONJSONDecodeCallbacks callbacks;
    initBonjsonDecodeCallbacks(&callbacks, decodeOnly);

    ksbonjson_decodeStatus status = ksbonjson_decode(document, documentSize, &callbacks, ctx, &decodedOffset);
    if(status != KSBONJSON_DECODE_OK)
    {
        printErrorAndExit("Failed to decode BONJSON file %s at offset %d: status %d (%s)",
                        src_path,
                        decodedOffset,
                        status,
                        ksbonjson_describeDecodeStatus(status));
    }

    ksbonjson_endEncode(&ctx->encoderContext);

    file = openFileForWriting(dst_path);
    writeToFile(file, ctx->buffer, ctx->pos);
    closeFile(file);
    freeBonjsonContext(ctx);
}


// ============================================================================
// Startup & command line args
// ============================================================================

#define QUOTE(str) #str
#define EXPAND_AND_QUOTE(str) QUOTE(str)

static char* g_argv_0;

static void printUsage(void)
{
    printError("\
Purpose:   Convert BONJSON to BONJSON for benchmarking.\n\
Version:   %s\n\
Copyright: (c) 2025 Karl Stenerud\n\
License:   MIT, NO WARRANTIES IMPLIED\n\
\n\
Usage: %s [options]\n\
Where the default behavior is to convert from stdin to stdout.\n\
\n\
Options:\n\
  -h: Print help and exit\n\
  -v: Print version and exit\n\
  -d: Decode only\n\
  -i <path>: Input file (use - to specify stdin) (default stdin)\n\
  -o <path>: Output file (use - to specify stdout) (default stdout)\n\
\n\
", EXPAND_AND_QUOTE(PROJECT_VERSION), basename(g_argv_0));
}

static void printVersion(void)
{
    printf("%s\n", EXPAND_AND_QUOTE(PROJECT_VERSION));
}


// ============================================================================
// Main
// ============================================================================

int main(const int argc, char** const argv)
{
    const char* src_path = "-";
    const char* dst_path = "-";

    g_argv_0 = argv[0];
    bool decodeOnly = false;

    int ch;
    while((ch = getopt(argc, argv, "?hvi:o:d")) >= 0)
    {
        switch(ch)
        {
            case '?':
            case 'h':
                printUsage();
                exit(0);
            case 'v':
                printVersion();
                exit(0);
            case 'i':
                src_path = strdup(optarg);
                break;
            case 'o':
                dst_path = strdup(optarg);
                break;
            case 'd':
                decodeOnly = true;
                break;
            default:
                printf("Unknown option: %d %c\n", ch, ch);
                printUsage();
                exit(1);
        }
    }

    bonjsonToBonjson(src_path, dst_path, decodeOnly);

    return 0;
}
