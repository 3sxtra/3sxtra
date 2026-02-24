/**
 * @file ramcnt.c
 * @brief RAM key management — alloc, free, search, and purge.
 *
 * Manages a pool of `RCKeyWork` entries backed by a heap allocator.
 * Each key represents a memory allocation with an associated type
 * and optional texture-group linkage. Used for loading character data,
 * stage data, and other dynamically-allocated resources.
 *
 * Part of the system module.
 * Originally from the PS2 ramcnt module.
 */

#include "sf33rd/Source/Game/system/ramcnt.h"
#include "common.h"
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Common/MemMan.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/rendering/texgroup.h"

#define ERR_STOP                                                                                                       \
    do {                                                                                                               \
        flLogOut("[ramcnt] ERR_STOP triggered at %s:%d", __FILE__, __LINE__);                                          \
        return;                                                                                                        \
    } while (0)
#define ERR_STOP_VAL(v)                                                                                                \
    do {                                                                                                               \
        flLogOut("[ramcnt] ERR_STOP triggered at %s:%d", __FILE__, __LINE__);                                          \
        return (v);                                                                                                    \
    } while (0)

RCKeyWork rckey_work[RCKEY_WORK_MAX];
_MEMMAN_OBJ rckey_mmobj;
s16 rckeyque[RCKEY_WORK_MAX];
s16 rckeyctr;
s16 rckeymin;

/** @brief Display debug overlay showing RAM key pool status (remaining memory, key count). */
void disp_ramcnt_free_area() {
    if (Debug_w[DEBUG_RAMCNT_FREE_AREA]) {
        flPrintColor(0xFFFFFF8F);
        flPrintL(4, 8, "Ramcnt Status");
        flPrintL(4, 9, "Now %07X", mmGetRemainder(&rckey_mmobj));
        flPrintL(4, 0xA, "Min %07X", mmGetRemainderMin(&rckey_mmobj));
        flPrintL(4, 0xB, "Key %2d / %2d", rckeymin, rckeyctr);
    }
}

/** @brief Initialize the RAM key pool: set up the heap and clear all key work entries. */
void Init_ram_control_work(u8* adrs, s32 size) {
    s16 i;

    mmHeapInitialize(&rckey_mmobj, adrs, size, ALIGN_UP(sizeof(_MEMMAN_CELL), 64), "- for Ramcnt -");

    for (i = 0; i < (RCKEY_WORK_MAX - 1); i++) {
        rckeyque[i] = ((RCKEY_WORK_MAX - 1) - i);
    }

    rckeyctr = (RCKEY_WORK_MAX - 1);
    rckeymin = (RCKEY_WORK_MAX - 1);
    rckeyque[rckeyctr] = 0;

    for (i = 0; i < sizeof(RCKeyWork) / 4; i++) {
        ((s32*)rckey_work)[i] = 0;
    }

    for (i = 1; i < RCKEY_WORK_MAX; i++) {
        rckey_work[i] = rckey_work[0];
    }
}

/** @brief Release a RAM key (non-texcash type) — free its memory and return the key to the pool. */
void Push_ramcnt_key(s16 key) {
    RCKeyWork* rwk = &rckey_work[key];

    if (rwk->use != 0) {
        if ((rwk->type == 8) || (rwk->type == 9)) {
            flLogOut("TEXCASH KEY PUSH ERROR\n");
            ERR_STOP;
        }

        Push_ramcnt_key_original_2(key);
    }

    return;
}

/** @brief Release a RAM key (texcash type only) — free its memory and return the key to the pool. */
void Push_ramcnt_key_original(s16 key) {
    RCKeyWork* rwk = &rckey_work[key];

    if (rwk->use != 0) {
        if ((rwk->type != 8) && (rwk->type != 9)) {
            flLogOut("TEXCASH KEY PUSH ERROR2\n");
            ERR_STOP;
        }

        Push_ramcnt_key_original_2(key);
    }

    return;
}

/** @brief Core key release: free heap memory, purge texture group, return key to queue. */
void Push_ramcnt_key_original_2(s16 key) {
    RCKeyWork* rwk = &rckey_work[key];

    if (rwk->use != 0) {
        mmFree(&rckey_mmobj, (u8*)rwk->adr);
        rwk->type = 0;
        rwk->use = 0;

        if (rwk->group_num) {
            purge_texture_group(rwk->group_num);
        }

        rwk->group_num = 0;
        rckeyque[rckeyctr++] = key;
    }
}

/** @brief Release all RAM keys whose type matches the given kind-of-key. */
void Purge_memory_of_kind_of_key(u8 kokey) {
    RCKeyWork* rwk;
    s16 i;

    for (i = 0; i < RCKEY_WORK_MAX; i++) {
        rwk = &rckey_work[i];

        if (rwk->use && (rwk->type == kokey)) {
            Push_ramcnt_key(i);
        }
    }
}

/** @brief Store a file size into the given RAM key entry. */
void Set_size_data_ramcnt_key(s16 key, u32 size) {
    if (key <= 0) {
        // An attempt was made to store a file size in an unused memory key.\n
        flLogOut("未使用のメモリキーへファイルサイズを格納しようとしました。\n");
        ERR_STOP;
    }

    rckey_work[key].size = size;
}

/** @brief Retrieve the stored file size from the given RAM key entry. */
size_t Get_size_data_ramcnt_key(s16 key) {
    if (key <= 0) {
        // An attempt was made to get a file size from an unused memory key.\n
        flLogOut("未使用のメモリキーからファイルサイズを取得しようとしました。\n");
        ERR_STOP_VAL(0);
    }

    return rckey_work[key].size;
}

/** @brief Retrieve the memory address stored in the given RAM key entry. */
uintptr_t Get_ramcnt_address(s16 key) {
    if (key <= 0) {
        // An attempt was made to obtain an address from an unused memory key.\n
        flLogOut("未使用のメモリキーからアドレスを取得しようとしました。\n");
        ERR_STOP_VAL(0);
    }

    return rckey_work[key].adr;
}

/** @brief Search for the first active RAM key matching the given type; returns key index or 0. */
s16 Search_ramcnt_type(u8 kokey) {
    s16 i;

    for (i = 1; i < RCKEY_WORK_MAX; i++) {
        if ((rckey_work[i].use) && (kokey == (rckey_work[i].type))) {
            return i;
        }
    }

    return 0;
}

/** @brief Test whether a RAM key is unused (0) or its texture group has been loaded. */
s32 Test_ramcnt_key(s16 key) {
    if (key == 0) {
        return 1;
    }

    if (rckey_work[key].use == 0) {
        return 0;
    }

    if (rckey_work[key].group_num != 0) {
        return 0;
    }

    return 1;
}

/** @brief Allocate a new RAM key with the requested memory size, type, and texture group. */
s16 Pull_ramcnt_key(size_t memreq, u8 kokey, u8 group, u8 frre) {
    RCKeyWork* rwk;
    s16 key;

    if (rckeyctr <= 0) {
        // There are not enough memory keys.\n
        flLogOut("メモリキーの個数が足りなくなりました。\n");
        ERR_STOP_VAL(-1);
    }

    key = rckeyque[(rckeyctr -= 1)];
    rwk = &rckey_work[key];

    if (rckeyctr < rckeymin) {
        rckeymin = rckeyctr;
    }

    if (memreq != 0) {
        rwk->size = memreq;

        if (frre != 0) {
            frre--;
        }

        rwk->adr = (uintptr_t)mmAlloc(&rckey_mmobj, memreq, frre);
    } else {
        goto err;
    }

    if (rwk->adr == 0) {
    err:
        rckeyque[rckeyctr++] = key;
        // Failed to allocate memory.\n
        flLogOut("メモリの確保に失敗しました。\n");
        ERR_STOP_VAL(-1);
    }

    rwk->use = 1;
    rwk->type = kokey;
    rwk->group_num = group;
    return key;
}
