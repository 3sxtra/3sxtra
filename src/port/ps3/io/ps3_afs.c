#include <cell/cell_fs.h>
#include <cell/sysmodule.h>
#include <sys/synchronization.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "port/io/afs.h"

#define AFS_MAGIC 0x41465300
#define AFS_ATTRIBUTE_HEADER_SIZE 8
#define AFS_ATTRIBUTE_ENTRY_SIZE 48
#define AFS_MAX_NAME_LENGTH 32

#define AFS_MAX_READ_REQUESTS 100

/* MRU Cache: 4 slots, max 64KB each */
#define AFS_CACHE_SLOTS 4
#define AFS_CACHE_MAX_SIZE (64 * 1024)

typedef struct {
    int file_num;       /* AFS file index cached, -1 = empty */
    void* data;         /* Cached data (memalign'd to 16) */
    uint32_t size;      /* Size of cached data */
    uint32_t age;       /* Incremented on every cache miss; lower = older */
} AFSCacheSlot;

static AFSCacheSlot s_cache[AFS_CACHE_SLOTS];
static uint32_t s_cache_age = 0;

static char* local_strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* d = malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}

static unsigned int ReadU32LE(int fd) {
    unsigned char b[4];
    uint64_t read_bytes = 0;
    if (cellFsRead(fd, b, 4, &read_bytes) != CELL_FS_SUCCEEDED || read_bytes != 4) return 0;
    return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
}

static unsigned int ReadU32BE(int fd) {
    unsigned char b[4];
    uint64_t read_bytes = 0;
    if (cellFsRead(fd, b, 4, &read_bytes) != CELL_FS_SUCCEEDED || read_bytes != 4) return 0;
    return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

static void read_string(int fd, char* dst) {
    uint64_t read_bytes = 0;
    if (cellFsRead(fd, dst, AFS_MAX_NAME_LENGTH, &read_bytes) != CELL_FS_SUCCEEDED || read_bytes != AFS_MAX_NAME_LENGTH) {
        memset(dst, 0, AFS_MAX_NAME_LENGTH);
    }
}

typedef struct AFSEntry {
    unsigned int offset;
    unsigned int size;
    char name[AFS_MAX_NAME_LENGTH];
} AFSEntry;

typedef struct AFS {
    char* file_path;
    int disk_fd; // Use File Descriptor for libfs
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

static AFS afs = { .file_path = NULL, .disk_fd = -1, .entry_count = 0, .entries = NULL };
static ReadRequest requests[AFS_MAX_READ_REQUESTS] = { { 0 } };
static sys_mutex_t read_mutex;
static int mutex_initialized = 0;

// F-HIGH-01 Audit Fix: Separate fd and mutex for audio thread reads
// to eliminate serialization of audio reads behind the main thread.
static int audio_disk_fd = -1;
static sys_mutex_t audio_read_mutex;
static int audio_mutex_initialized = 0;

static bool is_valid_attribute_data(unsigned int attributes_offset, unsigned int attributes_size, long file_size,
                                    unsigned int entries_end_offset, unsigned int entry_count) {
    if ((attributes_offset == 0) || (attributes_size == 0)) return false;
    if (attributes_size > (file_size - entries_end_offset)) return false;
    if (attributes_size < (entry_count * AFS_ATTRIBUTE_ENTRY_SIZE)) return false;
    if (attributes_offset < entries_end_offset) return false;
    if (attributes_offset > (file_size - attributes_size)) return false;
    return true;
}

/* --- MRU Cache helpers --- */

static void cache_init(void) {
    for (int i = 0; i < AFS_CACHE_SLOTS; i++) {
        s_cache[i].file_num = -1;
        s_cache[i].data = NULL;
        s_cache[i].size = 0;
        s_cache[i].age = 0;
    }
    s_cache_age = 0;
}

static AFSCacheSlot* cache_find(int file_num) {
    for (int i = 0; i < AFS_CACHE_SLOTS; i++) {
        if (s_cache[i].file_num == file_num && s_cache[i].data != NULL) {
            return &s_cache[i];
        }
    }
    return NULL;
}

static AFSCacheSlot* cache_evict(void) {
    /* Find the oldest (lowest age) slot */
    int oldest = 0;
    for (int i = 1; i < AFS_CACHE_SLOTS; i++) {
        if (s_cache[i].age < s_cache[oldest].age) {
            oldest = i;
        }
    }
    AFSCacheSlot* slot = &s_cache[oldest];
    if (slot->data) {
        free(slot->data);
        slot->data = NULL;
    }
    slot->file_num = -1;
    slot->size = 0;
    return slot;
}

static void cache_store(int file_num, const void* data, uint32_t size) {
    if (size > AFS_CACHE_MAX_SIZE) return; /* Too large to cache */

    AFSCacheSlot* slot = cache_find(file_num);
    if (slot) {
        slot->age = ++s_cache_age;
        return; /* Already cached */
    }

    slot = cache_evict();
    slot->data = memalign(16, (size + 15) & ~15);
    if (!slot->data) return;
    memcpy(slot->data, data, size);
    slot->file_num = file_num;
    slot->size = size;
    slot->age = ++s_cache_age;
}

static void cache_destroy(void) {
    for (int i = 0; i < AFS_CACHE_SLOTS; i++) {
        if (s_cache[i].data) {
            free(s_cache[i].data);
            s_cache[i].data = NULL;
        }
        s_cache[i].file_num = -1;
    }
}

/* --- End MRU Cache --- */

static bool init_afs(const char* file_path) {
    // F-01 Audit Fix: Only load FS module once to prevent reference count leaks
    static int fs_module_loaded = 0;
    if (!fs_module_loaded) {
        cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
        fs_module_loaded = 1;
    }

    // NEW-11: Close any previously open AFS to prevent fd leaks on re-init
    if (afs.disk_fd >= 0) {
        int close_ret = cellFsClose(afs.disk_fd);
        // F-LOW-01 Audit Fix: Check cellFsClose return value
        if (close_ret != CELL_FS_SUCCEEDED) {
            printf("[AFS] Warning: cellFsClose failed for fd %d (0x%x)\n", afs.disk_fd, close_ret);
        }
        afs.disk_fd = -1;
    }
    // F-HIGH-01: Close audio fd on re-init
    if (audio_disk_fd >= 0) {
        cellFsClose(audio_disk_fd);
        audio_disk_fd = -1;
    }
    if (afs.file_path) {
        free(afs.file_path);
        afs.file_path = NULL;
    }
    if (afs.entries) {
        free(afs.entries);
        afs.entries = NULL;
    }
    afs.entry_count = 0;

    char full_path[1024];
    if (file_path[0] != '/') {
        snprintf(full_path, sizeof(full_path), "/dev_hdd0/game/3SX00001/USRDIR/%s", file_path);
    } else {
        // F-03 Audit Fix: strncpy doesn't guarantee null-termination; use snprintf
        snprintf(full_path, sizeof(full_path), "%s", file_path);
    }

    afs.file_path = local_strdup(full_path);
    printf("init_afs: opening file %s via libfs\n", full_path);

    int fd;
    if (cellFsOpen(full_path, CELL_FS_O_RDONLY, &fd, NULL, 0) != CELL_FS_SUCCEEDED) {
        printf("init_afs: cellFsOpen failed for %s\n", full_path);
        return false;
    }

    CellFsStat stat;
    cellFsFstat(fd, &stat);
    long file_size = stat.st_size;
    printf("init_afs: opened %s, size = %ld\n", full_path, file_size);

    uint64_t pos = 0;
    unsigned int magic = ReadU32BE(fd);
    if (magic != AFS_MAGIC) {
        printf("init_afs: magic mismatch! expected %x, got %x\n", AFS_MAGIC, magic);
        cellFsClose(fd);
        return false;
    }

    afs.entry_count = ReadU32LE(fd);
    afs.entries = malloc(sizeof(AFSEntry) * afs.entry_count);
    if (!afs.entries) {
        cellFsClose(fd);
        return false;
    }
    memset(afs.entries, 0, sizeof(AFSEntry) * afs.entry_count);

    unsigned int entries_start_offset = 0;
    unsigned int entries_end_offset = 0;

    for (unsigned int i = 0; i < afs.entry_count; i++) {
        AFSEntry* entry = &afs.entries[i];
        entry->offset = ReadU32LE(fd);
        entry->size = ReadU32LE(fd);

        if (entry->offset != 0) {
            if (entries_start_offset == 0) {
                entries_start_offset = entry->offset;
            }
            entries_end_offset = entry->offset + entry->size;
        }
    }

    unsigned int attributes_offset = ReadU32LE(fd);
    unsigned int attributes_size = ReadU32LE(fd);
    bool has_attributes = false;

    if (is_valid_attribute_data(attributes_offset, attributes_size, file_size, entries_end_offset, afs.entry_count)) {
        has_attributes = true;
    } else {
        cellFsLseek(fd, entries_start_offset - AFS_ATTRIBUTE_HEADER_SIZE, CELL_FS_SEEK_SET, &pos);
        attributes_offset = ReadU32LE(fd);
        attributes_size = ReadU32LE(fd);

        if (is_valid_attribute_data(attributes_offset, attributes_size, file_size, entries_end_offset, afs.entry_count)) {
            has_attributes = true;
        }
    }

    for (unsigned int i = 0; i < afs.entry_count; i++) {
        AFSEntry* entry = &afs.entries[i];
        if ((entry->offset != 0) && has_attributes) {
            cellFsLseek(fd, attributes_offset + i * AFS_ATTRIBUTE_ENTRY_SIZE, CELL_FS_SEEK_SET, &pos);
            read_string(fd, entry->name);
        } else {
            memset(entry->name, 0, sizeof(entry->name));
        }
    }

    afs.disk_fd = fd;
    sys_mutex_attribute_t mutex_attr;
    sys_mutex_attribute_initialize(mutex_attr);
    mutex_attr.attr_recursive = SYS_SYNC_RECURSIVE;
    sys_mutex_create(&read_mutex, &mutex_attr);
    mutex_initialized = 1;

    // F-HIGH-01 Audit Fix: Open a second fd for audio thread reads
    int audio_fd;
    if (cellFsOpen(full_path, CELL_FS_O_RDONLY, &audio_fd, NULL, 0) == CELL_FS_SUCCEEDED) {
        audio_disk_fd = audio_fd;
        sys_mutex_attribute_t audio_mutex_attr;
        sys_mutex_attribute_initialize(audio_mutex_attr);
        audio_mutex_attr.attr_recursive = SYS_SYNC_RECURSIVE;
        sys_mutex_create(&audio_read_mutex, &audio_mutex_attr);
        audio_mutex_initialized = 1;
    } else {
        printf("[AFS] Warning: Could not open second fd for audio, reads will be serialized\n");
        audio_disk_fd = -1;
    }

    cache_init();

    return true;
}

bool AFS_Init(const char* file_path) {
    fprintf(stderr, "AFS_Init called with %s\n", file_path);
    fflush(stderr);
    if (!init_afs(file_path)) {
        fprintf(stderr, "AFS_Init failed!\n");
        return false;
    }
    fprintf(stderr, "AFS_Init success! %u entries loaded.\n", afs.entry_count);
    return true;
}

void AFS_Finish(void) {
    cache_destroy();
    if (afs.disk_fd >= 0) {
        int close_ret = cellFsClose(afs.disk_fd);
        // F-LOW-01 Audit Fix: Check cellFsClose return value
        if (close_ret != CELL_FS_SUCCEEDED) {
            printf("[AFS] Warning: cellFsClose failed for fd %d (0x%x)\n", afs.disk_fd, close_ret);
        }
        afs.disk_fd = -1;
    }
    // F-HIGH-01: Close audio fd
    if (audio_disk_fd >= 0) {
        cellFsClose(audio_disk_fd);
        audio_disk_fd = -1;
    }
    if (audio_mutex_initialized) {
        sys_mutex_destroy(audio_read_mutex);
        audio_mutex_initialized = 0;
    }
    if (mutex_initialized) {
        sys_mutex_destroy(read_mutex);
        mutex_initialized = 0;
    }
    free(afs.file_path);
    free(afs.entries);
    memset(&afs, 0, sizeof(afs));
    afs.disk_fd = -1;
    memset(requests, 0, sizeof(requests));
}

unsigned int AFS_GetFileCount(void) {
    return afs.entry_count;
}

unsigned int AFS_GetSize(int file_num) {
    if ((file_num < 0) || (file_num >= (int)afs.entry_count)) return 0;
    return afs.entries[file_num].size;
}

void AFS_RunServer(void) {
}

AFSHandle AFS_Open(int file_num) {
    for (int i = 0; i < AFS_MAX_READ_REQUESTS; i++) {
        if (!requests[i].initialized) {
            requests[i].file_num = file_num;
            requests[i].sector = 0;
            requests[i].index = i;
            requests[i].state = AFS_READ_STATE_IDLE;
            requests[i].initialized = true;
            return i;
        }
    }
    return AFS_NONE;
}

void AFS_Read(AFSHandle handle, int sectors, void* buf) {
    if (handle < 0 || handle >= AFS_MAX_READ_REQUESTS) return;
    
    ReadRequest* request = &requests[handle];
    if (!request->initialized) return;

    if (afs.disk_fd < 0 || !mutex_initialized) {
        request->state = AFS_READ_STATE_ERROR;
        return;
    }

    // H-08 Audit Fix: Bounds check before accessing entries array
    if (request->file_num < 0 || request->file_num >= (int)afs.entry_count) {
        printf("[AFS] ERROR: file_num %d out of bounds (max %u)\n", request->file_num, afs.entry_count);
        request->state = AFS_READ_STATE_ERROR;
        return;
    }

    unsigned int file_size = afs.entries[request->file_num].size;
    unsigned long offset = afs.entries[request->file_num].offset + (request->sector * 2048);
    uint64_t size = (uint64_t)sectors * 2048;

    /* F-MED-01 Audit Fix: Acquire mutex BEFORE cache_find to prevent TOCTOU race
     * where audio thread's cache_store/cache_evict modifies the cache mid-read. */
    sys_mutex_lock(read_mutex, 0);

    /* Check MRU cache first (only for full-file reads starting at sector 0) */
    if (request->sector == 0 && file_size <= AFS_CACHE_MAX_SIZE) {
        AFSCacheSlot* cached = cache_find(request->file_num);
        if (cached && cached->size >= size) {
            memcpy(buf, cached->data, (size_t)size);
            request->sector += sectors;
            request->state = AFS_READ_STATE_FINISHED;
            sys_mutex_unlock(read_mutex);
            return;
        }
    }

    /* Mutex already held — proceed with disk read */
    uint64_t pos_seek = 0;
    uint64_t read_bytes = 0;
    int ret_lseek = cellFsLseek(afs.disk_fd, offset, CELL_FS_SEEK_SET, &pos_seek);
    int ret_read = cellFsRead(afs.disk_fd, buf, size, &read_bytes);

    if (ret_lseek != CELL_FS_SUCCEEDED || ret_read != CELL_FS_SUCCEEDED || read_bytes != size) {
        printf("[AFS] ERROR: Read failed! Handle=%d, File=%d, Offset=%lu, ReqSize=%lu, ReadSize=%lu, LSeekRet=0x%x, ReadRet=0x%x\n",
               handle, request->file_num, offset, (unsigned long)size, (unsigned long)read_bytes, ret_lseek, ret_read);
        request->state = AFS_READ_STATE_ERROR;
    } else {
        /* Cache small full-file reads for MRU reuse */
        if (request->sector == 0 && file_size <= AFS_CACHE_MAX_SIZE) {
            cache_store(request->file_num, buf, (uint32_t)read_bytes);
        }
        request->sector += sectors;
        request->state = AFS_READ_STATE_FINISHED;
    }
    
    sys_mutex_unlock(read_mutex);
}

void AFS_ReadSync(AFSHandle handle, int sectors, void* buf) {
    AFS_Read(handle, sectors, buf);
}

/* F-HIGH-01 Audit Fix: Audio-thread read path using separate fd + mutex.
 * Eliminates serialization of audio reads behind the main thread's disk I/O. */
void AFS_ReadSyncAudio(AFSHandle handle, int sectors, void* buf) {
    if (handle < 0 || handle >= AFS_MAX_READ_REQUESTS) return;
    ReadRequest* request = &requests[handle];
    if (!request->initialized) return;

    /* Fallback to normal path if audio fd not available */
    if (audio_disk_fd < 0 || !audio_mutex_initialized) {
        AFS_Read(handle, sectors, buf);
        return;
    }

    if (request->file_num < 0 || request->file_num >= (int)afs.entry_count) {
        request->state = AFS_READ_STATE_ERROR;
        return;
    }

    unsigned long offset = afs.entries[request->file_num].offset + (request->sector * 2048);
    uint64_t size = (uint64_t)sectors * 2048;

    sys_mutex_lock(audio_read_mutex, 0);

    uint64_t pos_seek = 0;
    uint64_t read_bytes = 0;
    int ret_lseek = cellFsLseek(audio_disk_fd, offset, CELL_FS_SEEK_SET, &pos_seek);
    int ret_read = cellFsRead(audio_disk_fd, buf, size, &read_bytes);

    if (ret_lseek != CELL_FS_SUCCEEDED || ret_read != CELL_FS_SUCCEEDED || read_bytes != size) {
        printf("[AFS] ERROR: Audio read failed! Handle=%d, File=%d\n", handle, request->file_num);
        request->state = AFS_READ_STATE_ERROR;
    } else {
        request->sector += sectors;
        request->state = AFS_READ_STATE_FINISHED;
    }

    sys_mutex_unlock(audio_read_mutex);
}

void AFS_Stop(AFSHandle handle) {
    (void)handle;
}

void AFS_Close(AFSHandle handle) {
    if (handle < 0 || handle >= AFS_MAX_READ_REQUESTS) return;
    memset(&requests[handle], 0, sizeof(ReadRequest));
}

AFSReadState AFS_GetState(AFSHandle handle) {
    if (handle < 0 || handle >= AFS_MAX_READ_REQUESTS) return AFS_READ_STATE_ERROR;
    return requests[handle].state;
}

unsigned int AFS_GetSectorCount(AFSHandle handle) {
    if (handle < 0 || handle >= AFS_MAX_READ_REQUESTS) return 0;
    ReadRequest* request = &requests[handle];
    // F-04 Audit Fix: Bounds check file_num before accessing entries (matches H-08 in AFS_Read)
    if (request->file_num < 0 || request->file_num >= (int)afs.entry_count) return 0;
    unsigned int size = afs.entries[request->file_num].size;
    return (size + 2048 - 1) / 2048;
}
