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
#include <json.h>

#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
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

static void replaceString(char** str, const char* replacement, size_t length)
{
    if(*str != NULL)
    {
        free(*str);
    }
    *str = calloc(1, length+1);
    memcpy(*str, replacement, length);
}


// ============================================================================
// JSON to BONJSON
// ============================================================================

typedef struct
{
    uint8_t* buffer;
    size_t size;
    size_t pos;
} bonjson_encode_context;

static bonjson_encode_context* newBonjsonEncodeContext(size_t buffer_size)
{
    bonjson_encode_context* const ctx = calloc(1, sizeof(bonjson_encode_context));
    ctx->buffer = calloc(1, buffer_size);
    ctx->size = buffer_size;
    ctx->pos = 0;
    return ctx;
}

static void freeBonjsonEncodeContext(bonjson_encode_context* ctx)
{
    free(ctx->buffer);
    free(ctx);
}

#define PROPAGATE_ENCODE_ERROR(CALL) \
    do \
    { \
        ksbonjson_encodeStatus propagated_result = CALL; \
        unlikely_if(propagated_result != KSBONJSON_ENCODE_OK) \
        { \
            return propagated_result; \
        } \
    } \
    while(0)

static ksbonjson_encodeStatus addEncodedDataCallback(const uint8_t* KSBONJSON_RESTRICT data,
                                              size_t dataLength,
                                              void* KSBONJSON_RESTRICT userData)
{
    bonjson_encode_context* const ctx = (bonjson_encode_context*)userData;
    if(ctx->pos + dataLength > ctx->size)
    {
        printErrorAndExit("BUG: BONJSON buffer was too small (%d + %d > %d)", ctx->pos, dataLength, ctx->size);
    }
    memcpy(ctx->buffer+ctx->pos, data, dataLength);
    ctx->pos += dataLength;

    return KSBONJSON_ENCODE_OK;
}

static ksbonjson_encodeStatus parseJsonObject(json_object *obj, KSBONJSONEncodeContext* ctx);
static ksbonjson_encodeStatus parseJsonArray(json_object *obj, KSBONJSONEncodeContext* ctx);

static ksbonjson_encodeStatus parseJsonElement(json_object *obj, KSBONJSONEncodeContext* ctx)
{
    switch (json_object_get_type(obj))
    {
        case json_type_array:
        {
            const size_t array_length = json_object_array_length(obj);
            PROPAGATE_ENCODE_ERROR(ksbonjson_beginArray(ctx, array_length, false));
            PROPAGATE_ENCODE_ERROR(parseJsonArray(obj, ctx));
            break;
        }

        case json_type_object:
        {
            const size_t pair_count = json_object_object_length(obj);
            PROPAGATE_ENCODE_ERROR(ksbonjson_beginObject(ctx, pair_count, false));
            PROPAGATE_ENCODE_ERROR(parseJsonObject(obj, ctx));
            break;
        }

        case json_type_null:
            PROPAGATE_ENCODE_ERROR(ksbonjson_addNull(ctx));
            break;
        case json_type_boolean:
            PROPAGATE_ENCODE_ERROR(ksbonjson_addBoolean(ctx, json_object_get_boolean(obj)));
            break;
        case json_type_double:
            PROPAGATE_ENCODE_ERROR(ksbonjson_addFloat(ctx, json_object_get_double(obj)));
            break;
        case json_type_int:
        {
            // Figure out if json-c is using an int64 or a uint64 so that
            // we don't end up with a truncated value.
            // Internally, it's determined by cint_type, but there's no
            // public API for it so we have to go through this song and dance >:(
            const int64_t asInt = json_object_get_int64(obj);
            const uint64_t asUint = json_object_get_uint64(obj);
            if(asInt >= 0 && asUint > (uint64_t)asInt)
            {
                PROPAGATE_ENCODE_ERROR(ksbonjson_addUnsignedInteger(ctx, asUint));
            }
            else
            {
                PROPAGATE_ENCODE_ERROR(ksbonjson_addSignedInteger(ctx, asInt));
            }
            break;
        }
        case json_type_string:
        {
            const char* const str = json_object_get_string(obj);
            PROPAGATE_ENCODE_ERROR(ksbonjson_addString(ctx, str, strlen(str)));
            break;
        }

        default:
            printErrorAndExit("Unknown JSON type %d", json_object_get_type(obj));
    }
    return KSBONJSON_ENCODE_OK;
}

static ksbonjson_encodeStatus parseJsonArray(json_object *obj, KSBONJSONEncodeContext* ctx)
{
    const int array_length = json_object_array_length(obj);
    for(int i = 0; i < array_length; i++)
    {
        PROPAGATE_ENCODE_ERROR(parseJsonElement(json_object_array_get_idx(obj, i), ctx));
    }
    return KSBONJSON_ENCODE_OK;
}

static ksbonjson_encodeStatus parseJsonObject(json_object *obj, KSBONJSONEncodeContext* ctx)
{
    json_object_object_foreach(obj, key, val)
    {
        PROPAGATE_ENCODE_ERROR(ksbonjson_addString(ctx, key, strlen(key)));
        parseJsonElement(val, ctx);
    }
    return KSBONJSON_ENCODE_OK;
}

static ksbonjson_encodeStatus jsonToBonjson(const char* src_path, const char* dst_path)
{
    FILE* file = openFileForReading(src_path);
    size_t documentSize = 0;
    const uint8_t* const document = readEntireFile(file, &documentSize);
    closeFile(file);

	json_tokener* const tokener = json_tokener_new_ex(JSON_TOKENER_DEFAULT_DEPTH);
    if(tokener == NULL)
    {
        printErrorAndExit("Failed to build tokener");
    }

    json_object* const root = json_tokener_parse_ex(tokener, (const char*)document, documentSize);
    json_tokener_free(tokener);
    if(root == NULL)
    {
        printErrorAndExit("Failed to parse JSON");
    }

    bonjson_encode_context* const ctx = newBonjsonEncodeContext(documentSize*2);
    KSBONJSONEncodeContext eContext;
    ksbonjson_beginEncode(&eContext, addEncodedDataCallback, ctx);
    const ksbonjson_encodeStatus status = parseJsonElement(root, &eContext);
    if(status != KSBONJSON_ENCODE_OK)
    {
        printErrorAndExit("Failed to convert JSON to BONJSON: status %d (%s)",
                        status,
                        ksbonjson_describeEncodeStatus(status));
    }

    file = openFileForWriting(dst_path);
    writeToFile(file, ctx->buffer, ctx->pos);
    closeFile(file);
    freeBonjsonEncodeContext(ctx);

    return KSBONJSON_ENCODE_OK;
}


// ============================================================================
// BONJSON to JSON
// ============================================================================

#define PROPAGATE_DECODE_ERROR(CALL) \
    do \
    { \
        const ksbonjson_decodeStatus propagated_result = CALL; \
        unlikely_if(propagated_result != KSBONJSON_DECODE_OK) \
        { \
            return propagated_result; \
        } \
    } \
    while(0)

typedef struct
{
    json_object* obj;
    bool isInObject;
    bool nextIsName;
} DecoderFrame;

typedef struct
{
    KSBONJSONDecodeCallbacks callbacks;
    char* nextName;
    char* chunkingString;
    size_t chunkingStringLength;
    DecoderFrame stack[KSBONJSON_MAX_CONTAINER_DEPTH];
    int stackIndex;
} DecoderContext;

static bool addToChunkingString(DecoderContext* ctx, const char* str, size_t length)
{
    size_t oldLength = ctx->chunkingStringLength;
    size_t newLength = oldLength + length;

    char* newString = realloc(ctx->chunkingString, newLength + 1);
    if(newString == NULL)
    {
        return false;
    }

    memcpy(newString + oldLength, str, length);
    newString[newLength] = 0; // Zero terminate

    ctx->chunkingString = newString;
    ctx->chunkingStringLength = newLength;

    return true;
}

static void freeChunkingString(DecoderContext* ctx)
{
    free(ctx->chunkingString);
    ctx->chunkingString = NULL;
    ctx->chunkingStringLength = 0;
}

static int addObject(DecoderContext* ctx, json_object* obj)
{
    DecoderFrame* const frame = &ctx->stack[ctx->stackIndex];
    if(frame->obj == NULL)
    {
        frame->obj = obj;
    }
    else if(frame->isInObject)
    {
        json_object_object_add(frame->obj, ctx->nextName, obj);
        frame->nextIsName = true;
    }
    else
    {
        json_object_array_add(frame->obj, obj);
    }
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onBoolean(bool value, void* userData)
{
    DecoderContext* const ctx = (DecoderContext*)userData;
    return addObject(ctx, json_object_new_boolean(value));
}

static ksbonjson_decodeStatus onUnsignedInteger(uint64_t value, void* userData)
{
    DecoderContext* const ctx = (DecoderContext*)userData;
    return addObject(ctx, json_object_new_uint64(value));
}

static ksbonjson_decodeStatus onSignedInteger(int64_t value, void* userData)
{
    DecoderContext* const ctx = (DecoderContext*)userData;
    return addObject(ctx, json_object_new_int64(value));
}

static ksbonjson_decodeStatus onFloat(double value, void* userData)
{
    DecoderContext* const ctx = (DecoderContext*)userData;
    return addObject(ctx, json_object_new_double(value));
}

static ksbonjson_decodeStatus onNull(void* userData)
{
    DecoderContext* const ctx = (DecoderContext*)userData;
    return addObject(ctx, json_object_new_null());
}

static ksbonjson_decodeStatus onString(const char* KSBONJSON_RESTRICT value,
                                       size_t length,
                                       void* KSBONJSON_RESTRICT userData)
{
    DecoderContext* const ctx = (DecoderContext*)userData;
    DecoderFrame* const frame = &ctx->stack[ctx->stackIndex];
    if(frame->nextIsName)
    {
        replaceString(&ctx->nextName, value, length);
        frame->nextIsName = false;
        return KSBONJSON_DECODE_OK;
    }
    else
    {
        return addObject(ctx, json_object_new_string_len(value, length));
    }
}

static ksbonjson_decodeStatus onStringChunk(const char* KSBONJSON_RESTRICT value,
                                            size_t length,
                                            bool isLastChunk,
                                            void* KSBONJSON_RESTRICT userData)
{
    DecoderContext* const ctx = (DecoderContext*)userData;

    addToChunkingString(ctx, value, length);
    const char* str = ctx->chunkingString;

    if(!isLastChunk)
    {
        return KSBONJSON_DECODE_OK;
    }

    ksbonjson_decodeStatus result = KSBONJSON_DECODE_OK;
    DecoderFrame* const frame = &ctx->stack[ctx->stackIndex];
    if(frame->nextIsName)
    {
        replaceString(&ctx->nextName, str, length);
        frame->nextIsName = false;
        result = KSBONJSON_DECODE_OK;
    }
    else
    {
        result = addObject(ctx, json_object_new_string_len(str, length));
    }

    freeChunkingString(ctx);

    return result;
}

static ksbonjson_decodeStatus onBeginObject(size_t elementCountHint, void* userData)
{
    MARK_UNUSED(elementCountHint);
    DecoderContext* const ctx = (DecoderContext*)userData;
    json_object* const obj = json_object_new_object();
    PROPAGATE_DECODE_ERROR(addObject(ctx, obj));
    ctx->stackIndex++;
    DecoderFrame* const frame = &ctx->stack[ctx->stackIndex];
    frame->obj = obj;
    frame->isInObject = true;
    frame->nextIsName = true;
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onBeginArray(size_t elementCountHint, void* userData)
{
    MARK_UNUSED(elementCountHint);
    DecoderContext* const ctx = (DecoderContext*)userData;
    json_object* const obj = json_object_new_array();
    PROPAGATE_DECODE_ERROR(addObject(ctx, obj));
    ctx->stackIndex++;
    DecoderFrame* const frame = &ctx->stack[ctx->stackIndex];
    frame->obj = obj;
    frame->isInObject = false;
    frame->nextIsName = false;
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onEndContainer(void* userData)
{
    DecoderContext* const ctx = (DecoderContext*)userData;
    ctx->stackIndex--;
    return KSBONJSON_DECODE_OK;
}

static ksbonjson_decodeStatus onEndData(void* userData)
{
    DecoderContext* const ctx = (DecoderContext*)userData;
    freeChunkingString(ctx);
    return KSBONJSON_DECODE_OK;
}

static void initDecoderContext(DecoderContext* ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->callbacks.onBeginArray = onBeginArray;
    ctx->callbacks.onBeginObject = onBeginObject;
    ctx->callbacks.onBoolean = onBoolean;
    ctx->callbacks.onEndContainer = onEndContainer;
    ctx->callbacks.onEndData = onEndData;
    ctx->callbacks.onFloat = onFloat;
    ctx->callbacks.onUnsignedInteger = onUnsignedInteger;
    ctx->callbacks.onSignedInteger = onSignedInteger;
    ctx->callbacks.onNull = onNull;
    ctx->callbacks.onString = onString;
    ctx->callbacks.onStringChunk = onStringChunk;
}

static void bonjsonToJson(const char* const src_path, const char* const dst_path, bool prettyPrint)
{
    DecoderContext ctx;
    initDecoderContext(&ctx);
    FILE* file = openFileForReading(src_path);
    size_t documentSize = 0;
    const uint8_t* document = readEntireFile(file, &documentSize);
    closeFile(file);
    size_t decodedOffset;

    ksbonjson_decodeStatus status = ksbonjson_decode(document, documentSize, &ctx.callbacks, &ctx, &decodedOffset);
    if(status != KSBONJSON_DECODE_OK)
    {
        freeChunkingString(&ctx);
        printErrorAndExit("Failed to decode BONJSON file %s at offset %d: status %d (%s)",
                        src_path,
                        decodedOffset,
                        status,
                        ksbonjson_describeDecodeStatus(status));
    }

    file = openFileForWriting(dst_path);
    const char* jsonDoc = json_object_to_json_string_ext(ctx.stack[0].obj, prettyPrint ? JSON_C_TO_STRING_PRETTY : JSON_C_TO_STRING_PLAIN);
    writeToFile(file, (const uint8_t*)jsonDoc, strlen(jsonDoc));
    if(prettyPrint)
    {
        const uint8_t newline = '\n';
        writeToFile(file, &newline, 1);
    }
    closeFile(file);
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
Purpose:   Convert JSON <-> BONJSON.\n\
Version:   %s\n\
Copyright: (c) 2024 Karl Stenerud\n\
License:   MIT, NO WARRANTIES IMPLIED\n\
\n\
Usage: %s [options]\n\
Where the default behavior is to convert from stdin to stdout.\n\
\n\
Options:\n\
  -h: Print help and exit\n\
  -v: Print version and exit\n\
  -i <path>: Input file (use - to specify stdin) (default stdin)\n\
  -o <path>: Output file (use - to specify stdout) (default stdout)\n\
  -b: Convert JSON to BONJSON (default)\n\
  -j: Convert BONJSON to JSON\n\
  -m: Convert BONJSON to minified JSON\n\
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
    bool toJson = false;
    bool prettyPrint = false;
    const char* src_path = "-";
    const char* dst_path = "-";

    g_argv_0 = argv[0];

    int ch;
    while((ch = getopt(argc, argv, "?hvbjmi:o:")) >= 0)
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
            case 'b':
                toJson = false;
                break;
            case 'j':
                toJson = true;
                prettyPrint = true;
                break;
            case 'm':
                toJson = true;
                prettyPrint = false;
                break;
            case 'i':
                src_path = strdup(optarg);
                break;
            case 'o':
                dst_path = strdup(optarg);
                break;
            default:
                printf("Unknown option: %d %c\n", ch, ch);
                exit(1);
        }
    }

    if(toJson)
    {
        bonjsonToJson(src_path, dst_path, prettyPrint);
    }
    else
    {
        jsonToBonjson(src_path, dst_path);
    }

    return 0;
}
