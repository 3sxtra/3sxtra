/* Minimal zlib shim for PS3 build
 * Provides the z_stream struct and inflate function declarations.
 * On PS3, actual decompression is handled by edgeZlib (SPU DMA).
 * This shim satisfies the z_stream type reference in zlibApp.c.
 */
#ifndef ZLIB_H
#define ZLIB_H

#include <stddef.h>

#define ZLIB_VERSION "1.2.11-ps3stub"
#define Z_OK 0
#define Z_STREAM_END 1
#define Z_NEED_DICT 2
#define Z_ERRNO (-1)
#define Z_NO_FLUSH 0

typedef unsigned char Byte;
typedef unsigned long uLong;
typedef unsigned int uInt;

typedef void* (*alloc_func)(void* opaque, uInt items, uInt size);
typedef void (*free_func)(void* opaque, void* address);

typedef struct z_stream_s {
    const Byte* next_in;
    uInt avail_in;
    uLong total_in;
    Byte* next_out;
    uInt avail_out;
    uLong total_out;
    const char* msg;
    struct internal_state* state;
    alloc_func zalloc;
    free_func zfree;
    void* opaque;
    int data_type;
    uLong adler;
    uLong reserved;
} z_stream;

int inflateInit_(z_stream* strm, const char* version, int stream_size);
int inflate(z_stream* strm, int flush);
int inflateEnd(z_stream* strm);

#endif /* ZLIB_H */
