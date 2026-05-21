#include "port/sound/spu.h"
#include "common.h"
#include "structs.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#define SWAP16(x) ((u16)((((u16)(x) & 0x00FF) << 8) | (((u16)(x) & 0xFF00) >> 8)))
#define SWAP32(x)                                                                                                      \
    ((u32)((((u32)(x) & 0x000000FF) << 24) | (((u32)(x) & 0x0000FF00) << 8) | (((u32)(x) & 0x00FF0000) >> 8) |         \
           (((u32)(x) & 0xFF000000) >> 24)))

static s8* loaded_phd[21] = { NULL };
static SoundEvent* loaded_tsb[21] = { NULL };

static const char* char_names[21] = { "se",   "pl00", "pl01", "pl02", "pl03", "pl04", "pl05",
                                      "pl06", "pl07", "pl08", "pl09", "pl10", "pl11", "pl12",
                                      "pl13", "pl14", "pl15", "pl16", "pl17", "pl18", "pl19" };

s8* LoadPHDData(int index) {
    if (index < 0 || index >= 21)
        return NULL;
    if (loaded_phd[index] != NULL)
        return loaded_phd[index];

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "assets/sound/%s.phd", char_names[index]);

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        printf("Failed to load %s\n", filepath);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    s8* data = (s8*)malloc(size);
    size_t read = fread(data, 1, size, f);
    fclose(f);

    if (read != (size_t)size) {
        printf("Partial read for %s: got %zu of %ld bytes\n", filepath, read, size);
        free(data);
        return NULL;
    }

    loaded_phd[index] = data;
    return data;
}

SoundEvent* LoadTSBData(int index) {
    if (index < 0 || index >= 21)
        return NULL;
    if (loaded_tsb[index] != NULL)
        return loaded_tsb[index];

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "assets/sound/%s.tsb", char_names[index]);

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        printf("Failed to load %s\n", filepath);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    SoundEvent* data = (SoundEvent*)malloc(size);
    size_t read = fread(data, 1, size, f);
    fclose(f);

    if (read != (size_t)size) {
        printf("Partial read for %s: got %zu of %ld bytes\n", filepath, read, size);
        free(data);
        return NULL;
    }

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    int num_events = size / sizeof(SoundEvent);
    for (int i = 0; i < num_events; i++) {
        data[i].pitch = SWAP16((u16)data[i].pitch);
        data[i].kofftime = SWAP32(data[i].kofftime);
        data[i].param0 = SWAP16(data[i].param0);
        data[i].param1 = SWAP16(data[i].param1);
        data[i].param2 = SWAP16(data[i].param2);
        data[i].param3 = SWAP16(data[i].param3);
        data[i].link = SWAP16(data[i].link);
    }
#endif

    loaded_tsb[index] = data;
    return data;
}

void UnloadSoundAssets(void) {
    for (int i = 0; i < 21; i++) {
        if (loaded_phd[i]) {
            free(loaded_phd[i]);
            loaded_phd[i] = NULL;
        }
        if (loaded_tsb[i]) {
            free(loaded_tsb[i]);
            loaded_tsb[i] = NULL;
        }
    }
}
