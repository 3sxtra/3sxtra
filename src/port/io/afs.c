/**
 * @file afs.c
 * @brief AFS archive reader with preloaded RAM cache and async I/O.
 *
 * Parses AFS archive headers, preloads non-BGM entries into RAM for
 * zero-copy reads, and streams BGM files asynchronously via SDL3
 * async I/O with a persistent file handle.
 */
#include "port/io/afs.h"
#include "common.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// Inspired by https://github.com/MaikelChan/AFSLib

#define AFS_MAGIC 0x41465300
#define AFS_ATTRIBUTE_HEADER_SIZE 8
#define AFS_ATTRIBUTE_ENTRY_SIZE 48
#define AFS_MAX_NAME_LENGTH 32

// BGM files are large and streamed — skip preloading to save RAM.
#define AFS_BGM_START_INDEX 91
#define AFS_BGM_END_INDEX 1362

#define AFS_MAX_READ_REQUESTS 100

// Uncomment this to enable debug prints
// #define AFS_DEBUG

typedef struct AFSEntry {
    unsigned int offset;
    unsigned int size;
    char name[AFS_MAX_NAME_LENGTH];
    void* data; // Non-NULL if preloaded into RAM
} AFSEntry;

typedef struct AFS {
    char* file_path;
    unsigned int entry_count;
    AFSEntry* entries;
} AFS;

typedef struct ReadRequest {
    bool initialized;
    int index;
    int file_num;
    int sector;
    AFSReadState state;
} ReadRequest;

static AFS afs = { 0 };
static SDL_AsyncIOQueue* asyncio_queue = NULL;
static ReadRequest requests[AFS_MAX_READ_REQUESTS] = { { 0 } };

// ⚡ Bolt: Persistent async I/O handle — opened once at init, reused for all reads.
// Previously, AFS_Read opened a new SDL_AsyncIOFromFile per read (10-50 reads/transition),
// costing ~10-30µs per open() syscall on RPi4 and leaking the file descriptor.
// With preloading, only BGM files still use this path.
static SDL_AsyncIO* persistent_asyncio = NULL;

static bool is_valid_attribute_data(Uint32 attributes_offset, Uint32 attributes_size, Sint64 file_size,
                                    Uint32 entries_end_offset, Uint32 entry_count) {
    if ((attributes_offset == 0) || (attributes_size == 0)) {
        return false;
    }

    if (attributes_size > (file_size - entries_end_offset)) {
        return false;
    }

    if (attributes_size < (entry_count * AFS_ATTRIBUTE_ENTRY_SIZE)) {
        return false;
    }

    if (attributes_offset < entries_end_offset) {
        return false;
    }

    if (attributes_offset > (file_size - attributes_size)) {
        return false;
    }

    return true;
}

static void read_string(SDL_IOStream* src, char* dst) {
    char c;

    do {
        SDL_ReadIO(src, &c, 1);
        *dst++ = c;
    } while (c != '\0');
}

static bool init_afs(const char* file_path) {
    afs.file_path = SDL_strdup(file_path);
    SDL_IOStream* io = SDL_IOFromFile(file_path, "rb");

    if (io == NULL) {
        return false;
    }

    // Check magic

    Uint32 magic = 0;
    SDL_ReadU32BE(io, &magic);

    if (magic != AFS_MAGIC) {
        SDL_CloseIO(io);
        return false;
    }

    // Read entries

    SDL_ReadU32LE(io, &afs.entry_count);
    afs.entries = SDL_calloc(afs.entry_count, sizeof(AFSEntry));

    Uint32 entries_start_offset = 0;
    Uint32 entries_end_offset = 0;

    for (int i = 0; i < afs.entry_count; i++) {
        AFSEntry* entry = &afs.entries[i];
        SDL_ReadU32LE(io, &entry->offset);
        SDL_ReadU32LE(io, &entry->size);

        if (entry->offset != 0) {
            if (entries_start_offset == 0) {
                entries_start_offset = entry->offset;
            }

            entries_end_offset = entry->offset + entry->size;
        }
    }

    // Locate attributes

    Uint32 attributes_offset;
    Uint32 attributes_size;
    bool has_attributes = false;

    SDL_ReadU32LE(io, &attributes_offset);
    SDL_ReadU32LE(io, &attributes_size);

    if (is_valid_attribute_data(
            attributes_offset, attributes_size, SDL_GetIOSize(io), entries_end_offset, afs.entry_count)) {
        has_attributes = true;
    } else {
        SDL_SeekIO(io, entries_start_offset - AFS_ATTRIBUTE_HEADER_SIZE, SDL_IO_SEEK_SET);

        SDL_ReadU32LE(io, &attributes_offset);
        SDL_ReadU32LE(io, &attributes_size);

        if (is_valid_attribute_data(
                attributes_offset, attributes_size, SDL_GetIOSize(io), entries_end_offset, afs.entry_count)) {
            has_attributes = true;
        }
    }

    for (int i = 0; i < afs.entry_count; i++) {
        AFSEntry* entry = &afs.entries[i];

        if ((entry->offset != 0) && has_attributes) {
            SDL_SeekIO(io, attributes_offset + i * AFS_ATTRIBUTE_ENTRY_SIZE, SDL_IO_SEEK_SET);
            read_string(io, &entry->name);
        } else {
            SDL_memset(&entry->name, 0, sizeof(entry->name));
        }
    }

    // Preload non-BGM files into RAM for zero-copy reads.
    // BGM files (indices 91-1362) are large and streamed via async I/O.
    for (int i = 0; i < afs.entry_count; i++) {
        if (i >= AFS_BGM_START_INDEX && i <= AFS_BGM_END_INDEX) {
            continue;
        }

        AFSEntry* entry = &afs.entries[i];

        if ((entry->offset != 0) && (entry->size > 0)) {
            const unsigned int sector_aligned_size = (entry->size + 2048 - 1) & ~(2048 - 1);
            entry->data = SDL_malloc(sector_aligned_size);

            if (entry->data) {
                SDL_SeekIO(io, entry->offset, SDL_IO_SEEK_SET);
                SDL_ReadIO(io, entry->data, sector_aligned_size);
            }
        }
    }

    SDL_CloseIO(io);
    return true;
}

static bool init_asyncio(const char* file_path) {
    asyncio_queue = SDL_CreateAsyncIOQueue();
    if (asyncio_queue == NULL) {
        return false;
    }

    // ⚡ Bolt: Open one persistent handle for the AFS file.
    // With preloading, only BGM files still use this async I/O path.
    persistent_asyncio = SDL_AsyncIOFromFile(file_path, "r");
    if (persistent_asyncio == NULL) {
        printf("SDL_AsyncIOFromFile error: %s\n", SDL_GetError());
        SDL_DestroyAsyncIOQueue(asyncio_queue);
        asyncio_queue = NULL;
        return false;
    }

    return true;
}

bool AFS_Init(const char* file_path) {
    if (!init_afs(file_path)) {
        return false;
    }

    return init_asyncio(file_path);
}

void AFS_Finish() {
    // ⚡ Bolt: Close the persistent async I/O handle before destroying the queue.
    if (persistent_asyncio != NULL) {
        SDL_CloseAsyncIO(persistent_asyncio, true, asyncio_queue, NULL);
        // Drain any remaining outcomes from the close task
        SDL_AsyncIOOutcome outcome;
        while (SDL_WaitAsyncIOResult(asyncio_queue, &outcome, 100)) {
            // Process but discard
        }
        persistent_asyncio = NULL;
    }

    // Free preloaded file data
    if (afs.entries) {
        for (int i = 0; i < afs.entry_count; i++) {
            if (afs.entries[i].data) {
                SDL_free(afs.entries[i].data);
            }
        }
    }

    SDL_free(afs.file_path);
    SDL_free(afs.entries);
    SDL_zero(afs);
    SDL_zeroa(requests);
    SDL_DestroyAsyncIOQueue(asyncio_queue);
    asyncio_queue = NULL;
}

unsigned int AFS_GetFileCount() {
    return afs.entry_count;
}

unsigned int AFS_GetSize(int file_num) {
    if ((file_num < 0) || (file_num >= afs.entry_count)) {
        return 0;
    }

    return afs.entries[file_num].size;
}

// AFS reading

static void process_asyncio_outcome(const SDL_AsyncIOOutcome* outcome) {
    ReadRequest* request = (ReadRequest*)outcome->userdata;

#if defined(AFS_DEBUG)
    printf("📂 %d: request complete (type = %d, result = %d, offset = 0x%llX, requested = 0x%llX, transferred = "
           "0x%llX)\n",
           request->index,
           outcome->type,
           outcome->result,
           outcome->offset,
           outcome->bytes_requested,
           outcome->bytes_transferred);
#endif

    switch (outcome->type) {
    case SDL_ASYNCIO_TASK_READ:
        switch (outcome->result) {
        case SDL_ASYNCIO_COMPLETE:
            request->state = AFS_READ_STATE_FINISHED;
            break;

        case SDL_ASYNCIO_CANCELED:
            request->state = AFS_READ_STATE_IDLE;
            break;

        case SDL_ASYNCIO_FAILURE:
            request->state = AFS_READ_STATE_ERROR;
            break;
        }

        break;

    case SDL_ASYNCIO_TASK_CLOSE:
        request->state = AFS_READ_STATE_IDLE;
        break;

    case SDL_ASYNCIO_TASK_WRITE:
        // Do nothing
        break;
    }

#if defined(AFS_DEBUG)
    printf("📂 %d: new state = %d\n", request->index, request->state);
#endif
}

void AFS_RunServer() {
    SDL_AsyncIOOutcome outcome;

    while (SDL_GetAsyncIOResult(asyncio_queue, &outcome)) {
        process_asyncio_outcome(&outcome);
    }
}

AFSHandle AFS_Open(int file_num) {
    AFSHandle retval = AFS_NONE;

    for (int i = 0; i < SDL_arraysize(requests); i++) {
        ReadRequest* request = &requests[i];

        if (request->initialized) {
            continue;
        }

        request->file_num = file_num;
        request->sector = 0;
        request->index = i;
        request->state = AFS_READ_STATE_IDLE;
        request->initialized = true;
        retval = i;
        break;
    }

#if defined(AFS_DEBUG)
    printf("📂 %d: open (file_num = %d, filename = %s)\n", retval, file_num, afs.entries[file_num].name);
#endif

    return retval;
}

void AFS_Read(AFSHandle handle, int sectors, void* buf) {
#if defined(AFS_DEBUG)
    printf("📂 %d: read (sectors = %d, bytes = 0x%X)\n", handle, sectors, sectors * 2048);
#endif

    ReadRequest* request = &requests[handle];
    AFSEntry* entry = &afs.entries[request->file_num];

    // Fast path: preloaded data — zero-copy memcpy, no I/O
    if (entry->data) {
        SDL_memcpy(buf, (Uint8*)entry->data + request->sector * 2048, sectors * 2048);
        request->sector += sectors;
        request->state = AFS_READ_STATE_FINISHED;
        return;
    }

    // Slow path: async I/O via persistent handle (BGM files only)
    const Uint64 offset = entry->offset + request->sector * 2048;

    request->state = AFS_READ_STATE_READING;

    // ⚡ Bolt: Reuse persistent handle — eliminates per-read open() syscall.
    const bool success = SDL_ReadAsyncIO(persistent_asyncio, buf, offset, sectors * 2048, asyncio_queue, request);

    if (!success) {
        printf("SDL_ReadAsyncIO error: %s\n", SDL_GetError());
        request->state = AFS_READ_STATE_ERROR;
        return;
    }

    request->sector += sectors;
}

void AFS_ReadSync(AFSHandle handle, int sectors, void* buf) {
#if defined(AFS_DEBUG)
    printf("📂 %d: read sync\n", handle);
#endif

    AFS_Read(handle, sectors, buf);

    // Fast path: preloaded data completes immediately
    if (requests[handle].state == AFS_READ_STATE_FINISHED) {
        return;
    }

    SDL_AsyncIOOutcome outcome;

    while (SDL_WaitAsyncIOResult(asyncio_queue, &outcome, -1)) {
        process_asyncio_outcome(&outcome);

        ReadRequest* request = (ReadRequest*)outcome.userdata;

        if (request->index == handle) {
            break;
        }
    }
}

void AFS_Stop(AFSHandle handle) {
#if defined(AFS_DEBUG)
    printf("📂 %d: stop\n", handle);
#endif

    ReadRequest* request = &requests[handle];

    // ⚡ Bolt: With persistent handle, Stop just resets state.
    // The persistent handle stays open for future reads.
    if (request->state == AFS_READ_STATE_READING) {
        request->state = AFS_READ_STATE_IDLE;
    }
}

void AFS_Close(AFSHandle handle) {
#if defined(AFS_DEBUG)
    printf("📂 %d: close\n", handle);
#endif

    ReadRequest* request = &requests[handle];
    AFS_Stop(handle);
    SDL_zerop(request);
}

AFSReadState AFS_GetState(AFSHandle handle) {
    ReadRequest* request = &requests[handle];

#if defined(AFS_DEBUG)
    printf("📂 %d: get state (%d)\n", handle, request->state);
#endif

    return request->state;
}

unsigned int AFS_GetSectorCount(AFSHandle handle) {
    ReadRequest* request = &requests[handle];
    const unsigned int size = afs.entries[request->file_num].size;
    return (size + 2048 - 1) / 2048;
}
