/**
 * @file mtrans.c
 * Main Graphics Rendering and Transformation Engine
 */

#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "common.h"
#include "port/tracy_zones.h"
#include "port/legacy_matrix.h"
#include "port/renderer.h"
#include "port/sdl/sdl_game_renderer.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Common/PPGFile.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/rendering/aboutspr.h"
#include "sf33rd/Source/Game/rendering/chren3rd.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
#include "sf33rd/Source/Game/rendering/texgroup.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "structs.h"

#include <SDL3/SDL.h>

// ⚡ Opt5: SIMDe for portable SIMD intrinsics (SSE+FMA on x86, NEON on ARM)
#include <simde/x86/sse.h>
#include <simde/x86/fma.h>

#define PRIO_BASE_SIZE 128
#define SPRITE_LAYERS_MAX 24 // Maximum number of sprite layers (matches MultiTexture mts[] array)

typedef struct {
    Sprite2* chip;      // Active buffer (written by CPU this frame)
    Sprite2* chip_buf0; // ⚡ Opt8: Double-buffer A
    Sprite2* chip_buf1; // ⚡ Opt8: Double-buffer B
    u16 sprTotal;
    u16 sprMax;
    s8 up[SPRITE_LAYERS_MAX];
    u8 buf_index;       // ⚡ Opt8: Current buffer index (0 or 1)
} SpriteChipSet;

// ⚡ Hash-based tile cache lookup — O(1) replacement for linear get_mltbuf16/32 scans.
// Direct-mapped hash table per MTS slot. Knuth multiplicative hash with linear probing.
#define MLT_HASH_BITS 8
#define MLT_HASH_SIZE (1 << MLT_HASH_BITS)
#define MLT_HASH_MASK (MLT_HASH_SIZE - 1)
#define MLT_HASH_EMPTY (-1)

// ⚡ Opt2P2: Persistent tile cache — boost tile lifetime on cache hit.
// Tiles survive TILE_CACHE_BOOST × base_lifetime frames of disuse before eviction.
// This eliminates redundant LZ77 decompression for frequently-reused tiles.
#define TILE_CACHE_BOOST 4

typedef struct {
    u32 code;  // PatternCode that was cached
    s32 slot;  // index into mltcsh16/32, or MLT_HASH_EMPTY
} MltHashEntry;

static MltHashEntry s_hash16[MULTITEXTURE_MAX][MLT_HASH_SIZE];
static MltHashEntry s_hash32[MULTITEXTURE_MAX][MLT_HASH_SIZE];

static inline u32 mlt_hash(u32 code) {
    return (code * 2654435761u) >> (32 - MLT_HASH_BITS);
}

// ⚡ Opt4: CG Frame Pre-built Tile Descriptor Cache
// Pre-computes the tile map walk (X/Y offsets, TEX dimensions, tile sizes) once per
// cg_number. Eliminates per-frame TileMapEntry[] parsing and TEX pointer resolution.
#define CG_CACHE_MAX_TILES 128  // Max tiles per CG frame (game max observed: ~60)
#define CG_TILE_CACHE_BITS 10
#define CG_TILE_CACHE_SLOTS (1 << CG_TILE_CACHE_BITS)
#define CG_TILE_CACHE_MASK (CG_TILE_CACHE_SLOTS - 1)

typedef struct {
    f32 cum_x;      // Accumulated X offset (positive direction, before flip)
    f32 cum_y;      // Accumulated Y offset (positive direction, before flip)
    s32 dw;         // Tile width  (from TEX.wh)
    s32 dh;         // Tile height (from TEX.wh)
    s32 wh;         // Tile class: 1, 2, or 4
    s32 size;       // Decoded tile size in bytes: (wh*wh) << 6
    u16 tile_code;  // trsptr->code (index into texture table)
    u16 attr;       // trsptr->attr (raw, before XOR with WORK flip)
    u8* tex_data;   // Pointer to compressed tile data (&((u8*)texptr)[1])
} CGTileDesc;

typedef struct {
    u32 key;            // cg_number, or 0 = empty
    u16 count;          // Number of tiles
    u8  group;          // obj_group_table index
    u8  _pad;
    CGTileDesc tiles[CG_CACHE_MAX_TILES];
} CGTileCacheEntry;

static CGTileCacheEntry s_cg_tile_cache[CG_TILE_CACHE_SLOTS];

static inline u32 cg_cache_hash(u32 cg_number) {
    return (cg_number * 2654435761u) >> (32 - CG_TILE_CACHE_BITS);
}

/** @brief Build tile descriptors for a CG number (cache miss path).
 *
 * Performs the tile map walk once, pre-computing cumulative X/Y offsets,
 * TEX dimensions, and tile data pointers. All values are stored in the
 * "unflipped" direction — flip is applied at render time.
 */
static CGTileCacheEntry* cg_build_tile_descs(u32 cg_number) {
    u32 h = cg_cache_hash(cg_number);
    // Linear probing — find empty slot or our slot
    for (u32 p = 0; p < CG_TILE_CACHE_SLOTS; p++) {
        u32 idx = (h + p) & CG_TILE_CACHE_MASK;
        CGTileCacheEntry* e = &s_cg_tile_cache[idx];
        if (e->key == 0 || e->key == cg_number) {
            // Use this slot
            s32 i = obj_group_table[cg_number];
            if (i == 0 || texgrplds[i].ok == 0) {
                e->key = cg_number;
                e->count = 0;
                e->group = (u8)i;
                return e;
            }

            u32 n = cg_number - texgrpdat[i].num_of_1st;
            u16* trsbas = (u16*)(texgrplds[i].trans_table + ((u32*)texgrplds[i].trans_table)[n]);
            u32* textbl = (u32*)texgrplds[i].texture_table;
            s32 count = *trsbas;
            trsbas++;
            TileMapEntry* trsptr = (TileMapEntry*)trsbas;

            e->key = cg_number;
            e->group = (u8)i;
            e->count = (count > CG_CACHE_MAX_TILES) ? CG_CACHE_MAX_TILES : (u16)count;

            f32 cx = 0.0f, cy = 0.0f;
            for (s32 t = 0; t < e->count; t++, trsptr++) {
                CGTileDesc* d = &e->tiles[t];
                // Accumulate offsets in the unflipped (positive) direction
                cx += trsptr->x;
                cy += trsptr->y;
                d->cum_x = cx;
                d->cum_y = cy;
                d->tile_code = trsptr->code;
                d->attr = trsptr->attr;

                // Resolve TEX pointer and parse dimensions
                TEX* texptr = (TEX*)((uintptr_t)textbl + ((u32*)textbl)[trsptr->code]);
                d->dw = (texptr->wh & 0xE0) >> 2;
                d->dh = (texptr->wh & 0x1C) * 2;
                d->wh = (texptr->wh & 3) + 1;
                d->size = (d->wh * d->wh) << 6;
                d->tex_data = &((u8*)texptr)[1];
            }
            return e;
        }
    }
    // Table full — shouldn't happen with 1024 slots. Return NULL (caller falls back).
    return NULL;
}

/** @brief Look up or build cached tile descriptors for a CG number.
 * Returns NULL if cg_number has no valid group data.
 */
static CGTileCacheEntry* cg_lookup_tile_descs(u32 cg_number) {
    u32 h = cg_cache_hash(cg_number);
    for (u32 p = 0; p < CG_TILE_CACHE_SLOTS; p++) {
        u32 idx = (h + p) & CG_TILE_CACHE_MASK;
        CGTileCacheEntry* e = &s_cg_tile_cache[idx];
        if (e->key == cg_number) {
            return e;  // Cache hit
        }
        if (e->key == 0) {
            break;  // Empty slot — miss
        }
    }
    // Cache miss — build and insert
    return cg_build_tile_descs(cg_number);
}

/** @brief Invalidate the CG tile descriptor cache.
 * Call when texture group data is reloaded. */
static void cg_cache_invalidate(void) {
    for (s32 i = 0; i < CG_TILE_CACHE_SLOTS; i++) {
        s_cg_tile_cache[i].key = 0;
    }
}

// sbss
s32 curr_bright;
SpriteChipSet seqs_w;

// bss
f32 PrioBase[PRIO_BASE_SIZE];
f32 PrioBaseOriginal[PRIO_BASE_SIZE];

// rodata
static const u16 flptbl[4] = { 0x0000, 0x8000, 0x4000, 0xC000 };

static const u32 bright_type[4][16] = { { 0x00FFFFFF,
                                          0x00EEEEEE,
                                          0x00DDDDDD,
                                          0x00CCCCCC,
                                          0x00BBBBBB,
                                          0x00AAAAAA,
                                          0x00999999,
                                          0x00888888,
                                          0x00777777,
                                          0x00666666,
                                          0x00555555,
                                          0x00444444,
                                          0x00333333,
                                          0x00222222,
                                          0x00111111,
                                          0x00000000 },
                                        { 0x00FFFFFF,
                                          0x00FFEEEE,
                                          0x00FFDDDD,
                                          0x00FFCCCC,
                                          0x00FFBBBB,
                                          0x00FFAAAA,
                                          0x00FF9999,
                                          0x00FF8888,
                                          0x00FF7777,
                                          0x00FF6666,
                                          0x00FF5555,
                                          0x00FF4444,
                                          0x00FF3333,
                                          0x00FF2222,
                                          0x00FF1111,
                                          0x00FF0000 },
                                        { 0x00FFFFFF,
                                          0x00EEFFEE,
                                          0x00DDFFDD,
                                          0x00CCFFCC,
                                          0x00BBFFBB,
                                          0x00AAFFAA,
                                          0x0099FF99,
                                          0x0088FF88,
                                          0x0077FF77,
                                          0x0066FF66,
                                          0x0055FF55,
                                          0x0044FF44,
                                          0x0033FF33,
                                          0x0022FF22,
                                          0x0011FF11,
                                          0x0000FF00 },
                                        { 0x00FFFFFF,
                                          0x00EEEEFF,
                                          0x00DDDDFF,
                                          0x00CCCCFF,
                                          0x00BBBBFF,
                                          0x00AAAAFF,
                                          0x009999FF,
                                          0x008888FF,
                                          0x007777FF,
                                          0x006666FF,
                                          0x005555FF,
                                          0x004444FF,
                                          0x003333FF,
                                          0x002222FF,
                                          0x001111FF,
                                          0x000000FF } };

// ⚡ Opt3: Cached matrix elements for inlined per-chip transform.
// Extracted once per character in mlt_obj_matrix(), used N times in seqsStoreChip().
// Since input z==0 for all chips, the z-row contributions are eliminated (saves 4 mults/chip).
static f32 s_mtx_00, s_mtx_10, s_mtx_tx;  // X row: out.x = in.x*m00 + in.y*m10 + tx
static f32 s_mtx_01, s_mtx_11, s_mtx_ty;  // Y row: out.y = in.x*m01 + in.y*m11 + ty
static f32 s_mtx_02, s_mtx_12, s_mtx_tz;  // Z row: out.z = in.x*m02 + in.y*m12 + tz
static f32 s_mtx_z_step;                   // Per-chip Z increment (cmtx.a[2][2] * 1/65536)

// forward decls
static void DebugLine(f32 x, f32 y, f32 w, f32 h);
static s32 seqsStoreChip(f32 x, f32 y, s32 w, s32 h, s32 gix, s32 code, s32 attr, s32 alpha, s32 id);
static void appRenewTempPriority(s32 z);
static s16 check_patcash_ex_trans(PatternCollection* padr, u32 cg);
static s32 get_free_patcash_index(PatternCollection* padr);
static s32 get_mltbuf16(MultiTexture* mt, u32 code, u32 palt, s32* ret);

static s32 get_mltbuf16_ext_2(MultiTexture* mt, u32 code, u32 palt, s32* ret, PatternInstance* cp);
static s32 get_mltbuf32(MultiTexture* mt, u32 code, u32 palt, s32* ret);

static s32 get_mltbuf32_ext_2(MultiTexture* mt, u32 code, u32 palt, s32* ret, PatternInstance* cp);
static void lz_ext_p6_fx(u8* srcptr, u8* dstptr, u32 len);
static void lz_ext_p6_cx(u8* srcptr, u16* dstptr, u32 len, u16* palptr);
static u16 x16_mapping_set(PatternMap* map, s32 code);
static u16 x32_mapping_set(PatternMap* map, s32 code);

/**
 * @brief ⚡ Opt6: GPU compute LZ77 decode with automatic CPU fallback.
 *
 * Tries to enqueue the tile for GPU compute decompression. If that fails
 * (staging full, GPU unavailable, etc.), falls back to CPU decode + upload.
 *
 * @param src        Compressed data pointer
 * @param comp_bound Upper bound on compressed size
 * @param size       Expected decompressed byte count
 * @param mt         MultiTexture context
 * @param gix_base   Base gix (mt->mltgidx16 or mt->mltgidx32)
 * @param code       Tile code from get_mltbuf16/32
 * @param pal_bits   Palette index bits (palo or palt, lower 9 bits used)
 * @param is32       true if 32×32 tile (wh=4), false if 8/16×16
 */
static void lz77_gpu_or_cpu(u8* src, u32 comp_bound, u32 size,
                             MultiTexture* mt, s32 gix_base, s32 code,
                             s32 pal_bits, int is32) {
    s32 code_offset = is32 ? (code >> 6) : (code >> 8);
    s32 code_local  = is32 ? (code & 0x3F) : (code & 0xFF);
    u32 tile_dim    = is32 ? 32 : ((size == 0x40) ? 8 : 16);

    // Resolve ppg handles to match what seqsStoreChip/SetTexture will use
    s32 tex_handle = ppgGetUsingTextureHandle(NULL, gix_base + code_offset);
    s32 pal_handle = ppgGetUsingPaletteHandle(NULL, pal_bits & 0x1FF);

    if (tex_handle > 0 &&
        Renderer_LZ77Enqueue(src, comp_bound, size,
                              tex_handle, pal_handle,
                              (u32)code_local, tile_dim)) {
        return;  // GPU path succeeded
    }

    // CPU fallback
    lz_ext_p6_fx(src, mt->mltbuf, size);
    Renderer_UpdateTexture(gix_base + code_offset, mt->mltbuf, code_local, size, 0, 0);
}

/** @brief Replace tile map entries matching a source code/attribute with new values. */
static void search_trsptr(uintptr_t trstbl, s32 i, s32 n, s32 cods, s32 atrs, s32 codd, s32 atrd) {
    s32 j;
    u16* tmpbas;
    s32 ctemp;
    TileMapEntry* tmpptr;
    TileMapEntry* unused_s4;

    atrd &= 0x3FFF;

    for (j = i; j < n; j++) {
        tmpbas = (u16*)(trstbl + ((u32*)trstbl)[j]);
        ctemp = *tmpbas;
        tmpbas++;
        tmpptr = (TileMapEntry*)tmpbas;

        while (ctemp != 0) {
            if (!(tmpptr->attr & 0x1000) && (tmpptr->code == cods) && ((tmpptr->attr & 0xF) == atrs)) {
                tmpptr->code = codd;
                tmpptr->attr = (tmpptr->attr & 0xC000) | atrd;
            }

            ctemp--;
            unused_s4 = tmpptr;
            tmpptr = unused_s4 + 1;
        }
    }
}

/** @brief Render a multi-texture object with standard display mode. */
void mlt_obj_disp(MultiTexture* mt, WORK* wk, s32 base_y) {
    u16* trsbas;
    TileMapEntry* trsptr;
    s32 rnum;
    s32 attr;
    s32 palo;
    s32 count;
    s32 n;
    s32 i;
    f32 x;
    f32 y;
    s32 dw;
    s32 dh;

    ppgSetupCurrentDataList(&mt->texList);
    n = wk->cg_number;
    i = obj_group_table[n];

    if (i == 0) {
        return;
    }

    if (texgrplds[i].ok == 0) {
        // The trans data is not valid. Group number: %d\n
        flLogOut("トランスデータが有効ではありません。グループ番号：%d, cg_number: %d\n", i, n);
        return;
    }

    n -= texgrpdat[i].num_of_1st;
    trsbas = (u16*)(texgrplds[i].trans_table + ((u32*)texgrplds[i].trans_table)[n]);
    count = *trsbas;
    trsbas++;
    trsptr = (TileMapEntry*)trsbas;
    x = y = 0.0f;
    attr = flptbl[wk->cg_flip ^ wk->rl_flag];
    palo = wk->colcd & 0xF;

    if (wk->my_bright_type) {
        curr_bright = bright_type[(wk->my_bright_type - 1 < 4) ? wk->my_bright_type - 1 : 3]
                                 [(wk->my_bright_level < 16) ? wk->my_bright_level : 15];
    } else {
        curr_bright = 0xFFFFFF;
    }

    mlt_obj_matrix(wk, base_y);

    while (count--) {
        if (attr & 0x8000) {
            x += trsptr->x;
        } else {
            x -= trsptr->x;
        }

        if (attr & 0x4000) {
            y -= trsptr->y;
        } else {
            y += trsptr->y;
        }

        dw = ((trsptr->attr & 0xC00) >> 7) + 8;
        dh = ((trsptr->attr & 0x300) >> 5) + 8;

        if (!(trsptr->attr & 0x2000)) {
            if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                DebugLine(x - (dw & ((s16)attr >> 0x10)), y + (dh & ((s16)(attr * 2) >> 16)), dw, dh);
            }

            rnum = seqsStoreChip(x - (dw * BOOL(attr & 0x8000)),
                                 y + (dh * BOOL(attr & 0x4000)),
                                 dw,
                                 dh,
                                 mt->mltgidx16,
                                 trsptr->code,
                                 palo + ((trsptr->attr ^ attr) & 0xE00F),
                                 wk->my_clear_level,
                                 mt->id);
        } else {
            if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                DebugLine(x - (dw & ((s16)attr >> 0x10)), y + (dh & ((s16)(attr * 2) >> 16)), dw, dh);
            }

            rnum = seqsStoreChip(x - dw * BOOL(attr & 0x8000),
                                 y + dh * BOOL(attr & 0x4000),
                                 dw,
                                 dh,
                                 mt->mltgidx32,
                                 trsptr->code,
                                 palo + ((trsptr->attr ^ attr) & 0xE00F),
                                 wk->my_clear_level,
                                 mt->id);
        }

        if (rnum == 0) {
            break;
        }

        trsptr += 1;
    }

    seqs_w.up[mt->id] = 1;
    appRenewTempPriority(wk->position_z);
}

/** @brief Render a multi-texture object in RGB (unpaletted) mode. */
void mlt_obj_disp_rgb(MultiTexture* mt, WORK* wk, s32 base_y) {
    u16* trsbas;
    TileMapEntry* trsptr;
    s32 rnum;
    s32 attr;
    s32 count;
    s32 n;
    s32 i;
    f32 x;
    f32 y;
    s32 dw;
    s32 dh;

    ppgSetupCurrentDataList(&mt->texList);
    n = wk->cg_number;
    i = obj_group_table[n];

    if (i == 0) {
        return;
    }

    if (texgrplds[i].ok == 0) {
        // The trans data is not valid. Group number: %d\n
        flLogOut("トランスデータが有効ではありません。グループ番号：%d\n", i);
        return;
    }

    n -= texgrpdat[i].num_of_1st;
    trsbas = (u16*)(texgrplds[i].trans_table + ((u32*)texgrplds[i].trans_table)[n]);
    count = *trsbas;
    trsbas++;
    trsptr = (TileMapEntry*)trsbas;
    x = y = 0.0f;
    attr = flptbl[wk->cg_flip ^ wk->rl_flag];

    if (wk->my_bright_type) {
        curr_bright = bright_type[(wk->my_bright_type - 1 < 4) ? wk->my_bright_type - 1 : 3]
                                 [(wk->my_bright_level < 16) ? wk->my_bright_level : 15];
    } else {
        curr_bright = 0xFFFFFF;
    }

    mlt_obj_matrix(wk, base_y);

    while (count--) {
        if (attr & 0x8000) {
            x += trsptr->x;
        } else {
            x -= trsptr->x;
        }

        if (attr & 0x4000) {
            y -= trsptr->y;
        } else {
            y += trsptr->y;
        }

        dw = ((trsptr->attr & 0xC00) >> 7) + 8;
        dh = ((trsptr->attr & 0x300) >> 5) + 8;

        if (!(trsptr->attr & 0x2000)) {
            if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                DebugLine(x - (dw & ((s16)attr >> 0x10)), y + (dh & ((s16)(attr * 2) >> 16)), dw, dh);
            }

            rnum = seqsStoreChip(x - (dw * BOOL(attr & 0x8000)),
                                 y + (dh * BOOL(attr & 0x4000)),
                                 dw,
                                 dh,
                                 mt->mltgidx16,
                                 trsptr->code,
                                 (trsptr->attr ^ attr) & 0xE000,
                                 wk->my_clear_level,
                                 mt->id);
        } else {
            if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                DebugLine(x - (dw & ((s16)attr >> 0x10)), y + (dh & ((s16)(attr * 2) >> 16)), dw, dh);
            }

            rnum = seqsStoreChip(x - (dw * BOOL(attr & 0x8000)),
                                 y + (dh * BOOL(attr & 0x4000)),
                                 dw,
                                 dh,
                                 mt->mltgidx32,
                                 trsptr->code,
                                 (trsptr->attr ^ attr) & 0xE000,
                                 wk->my_clear_level,
                                 mt->id);
        }

        if (rnum == 0) {
            break;
        }

        trsptr++;
    }

    seqs_w.up[mt->id] = 1;
    appRenewTempPriority(wk->position_z);
}

/** @brief Calculate the maximum vertical extent of a sprite by CG number. */
s16 getObjectHeight(u16 cgnum) {
    s32 count;
    TileMapEntry* trsptr;
    s16 maxHeight;
    u16* trsbas;
    s32 i = obj_group_table[cgnum];
    s16 height;

    if (i == 0) {
        return 0;
    }

    if (texgrplds[i].ok == 0) {
        return 0;
    }

    cgnum -= texgrpdat[i].num_of_1st;
    trsbas = (u16*)((s8*)texgrplds[i].trans_table + ((u32*)texgrplds[i].trans_table)[cgnum]);
    count = *trsbas;
    trsbas++;
    trsptr = (TileMapEntry*)trsbas;

    for (maxHeight = height = 0; count--; trsptr++) {
        height = height + trsptr->y;

        if (height > maxHeight) {
            maxHeight = height;
        }
    }

    if (height) {
        // do nothing
    }

    return maxHeight;
}

/** @brief Transform and render with pattern caching (extended variant). */
void mlt_obj_trans_ext(MultiTexture* mt, WORK* wk, s32 base_y) {
    u32* textbl;
    u16* trsbas;
    TileMapEntry* trsptr;
    TEX* texptr;
    s32 rnum;
    s32 attr;
    s32 palo;
    s32 count;
    s32 n;
    s32 i;
    f32 x;
    f32 y;
    s16 ix;
    PatternCode cc;
    PatternInstance* cp;

    n = wk->cg_number;
    i = obj_group_table[n];

    if (i == 0) {
        return;
    }

    if (texgrplds[i].ok == 0) {
        // The trans data is not valid. Group number: %d\n
        flLogOut("トランスデータが有効ではありません。グループ番号：%d\n", i);
        return;
    }

    n -= texgrpdat[i].num_of_1st;
    trsbas = (u16*)(texgrplds[i].trans_table + ((u32*)texgrplds[i].trans_table)[n]);
    textbl = (u32*)texgrplds[i].texture_table;
    count = *trsbas;
    trsbas++;
    trsptr = (TileMapEntry*)trsbas;
    x = y = 0.0f;
    attr = flptbl[wk->cg_flip ^ wk->rl_flag];
    palo = wk->colcd;

    if (wk->my_bright_type) {
        curr_bright = bright_type[(wk->my_bright_type - 1 < 4) ? wk->my_bright_type - 1 : 3]
                                 [(wk->my_bright_level < 16) ? wk->my_bright_level : 15];
    } else {
        curr_bright = 0xFFFFFF;
    }

    mlt_obj_matrix(wk, base_y);
    cc.parts.group = 0;
    cc.parts.offset = wk->cg_number;
    ix = check_patcash_ex_trans(mt->cpat, cc.code);

    if (ix < 0) {
        {
            s32 size;
            s32 code;
            s32 wh;
            s32 dw;
            s32 dh;

            (void)dw;
            (void)dh;

            ix = get_free_patcash_index(mt->cpat);
            cp = &mt->cpat->patt[ix];
            mt->cpat->adr[mt->cpat->kazu] = cp;
            mt->cpat->kazu += 1;
            cp->curr_disp = 1;
            cp->time = mt->mltcshtime16;
            cp->cg.code = cc.code;
            cp->x16 = 0;
            cp->x32 = 0;
            SDL_zero(cp->map);
            cc.parts.group = i;

            while (count--) {
                if (attr & 0x8000) {
                    x += trsptr->x;
                } else {
                    x -= trsptr->x;
                }

                if (attr & 0x4000) {
                    y -= trsptr->y;
                } else {
                    y += trsptr->y;
                }

                texptr = (TEX*)((uintptr_t)textbl + ((u32*)textbl)[trsptr->code]);
                dw = (texptr->wh & 0xE0) >> 2;
                dh = (texptr->wh & 0x1C) * 2;
                wh = (texptr->wh & 3) + 1;
                size = (wh * wh) << 6;
                cc.parts.offset = trsptr->code;

                switch (wh) {
                case 1:
                case 2:
                    if (get_mltbuf16_ext_2(mt, cc.code, 0, &code, cp) != 0) {
                        lz77_gpu_or_cpu(&((u8*)texptr)[1], size * 2, size, mt, mt->mltgidx16, code, palo, 0);
                    }

                    if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                        DebugLine(x - (dw & ((s16)attr >> 0x10)), y + (dh & ((s16)(attr * 2) >> 16)), dw, dh);
                    }

                    rnum = seqsStoreChip(x - (dw * BOOL(attr & 0x8000)),
                                         y + (dh * BOOL(attr & 0x4000)),
                                         dw,
                                         dh,
                                         mt->mltgidx16,
                                         code,
                                         palo | ((trsptr->attr ^ attr) & 0xC000),
                                         wk->my_clear_level,
                                         mt->id);
                    break;

                case 4:
                    if (get_mltbuf32_ext_2(mt, cc.code, 0, &code, cp) != 0) {
                        lz77_gpu_or_cpu(&((u8*)texptr)[1], size * 2, size, mt, mt->mltgidx32, code, palo, 1);
                    }

                    if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                        DebugLine(x - (dw & ((s16)attr >> 0x10)), y + (dh & ((s16)(attr * 2) >> 16)), dw, dh);
                    }

                    rnum = seqsStoreChip(x - (dw * BOOL(attr & 0x8000)),
                                         y + (dh * BOOL(attr & 0x4000)),
                                         dw,
                                         dh,
                                         mt->mltgidx32,
                                         code,
                                         palo | (((trsptr->attr ^ attr) & 0xC000) | 0x2000),
                                         wk->my_clear_level,
                                         mt->id);
                    break;
                }

                if (rnum == 0) {
                    break;
                }

                trsptr++;
            }

            seqs_w.up[mt->id] = 1;
            appRenewTempPriority(wk->position_z);
            return;
        }
    }

    {
        s32 code;
        s32 size;
        s32 wh;
        s32 dw;
        s32 dh;

        (void)dw;
        (void)dh;

        cp = mt->cpat->adr[ix];
        cp->curr_disp = 1;
        cp->time = mt->mltcshtime16;
        cc.parts.group = i;

        while (count--) {
            if (attr & 0x8000) {
                x += trsptr->x;
            } else {
                x -= trsptr->x;
            }

            if (attr & 0x4000) {
                y -= trsptr->y;
            } else {
                y += trsptr->y;
            }

            texptr = (TEX*)((uintptr_t)textbl + ((u32*)textbl)[trsptr->code]);
            dw = (texptr->wh & 0xE0) >> 2;
            dh = (texptr->wh & 0x1C) * 2;
            wh = (texptr->wh & 3) + 1;
            size = (wh * wh) << 6;
            cc.parts.offset = trsptr->code;

            switch (wh) {
            case 1:
            case 2:
                if (get_mltbuf16_ext_2(mt, cc.code, 0, &code, cp) != 0) {
                    lz77_gpu_or_cpu(&((u8*)texptr)[1], size * 2, size, mt, mt->mltgidx16, code, palo, 0);
                }

                if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                    DebugLine(x - (dw & ((s16)attr >> 16)), y + (dh & ((s16)(attr * 2) >> 16)), dw, dh);
                }

                rnum = seqsStoreChip(x - (dw * BOOL(attr & 0x8000)),
                                     y + (dh * BOOL(attr & 0x4000)),
                                     dw,
                                     dh,
                                     mt->mltgidx16,
                                     code,
                                     palo | ((trsptr->attr ^ attr) & 0xC000),
                                     wk->my_clear_level,
                                     mt->id);
                break;

            case 4:
                if (get_mltbuf32_ext_2(mt, cc.code, 0, &code, cp) != 0) {
                    lz77_gpu_or_cpu(&((u8*)texptr)[1], size * 2, size, mt, mt->mltgidx32, code, palo, 1);
                }

                if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                    DebugLine(x - (dw & ((s16)attr >> 16)), y + (dh & ((s16)(attr * 2) >> 16)), dw, dh);
                }

                rnum = seqsStoreChip(x - (dw * BOOL(attr & 0x8000)),
                                     y + (dh * BOOL(attr & 0x4000)),
                                     dw,
                                     dh,
                                     mt->mltgidx32,
                                     code,
                                     palo | (((trsptr->attr ^ attr) & 0xC000) | 0x2000),
                                     wk->my_clear_level,
                                     mt->id);
                break;
            }

            if (rnum == 0) {
                break;
            }

            trsptr++;
        }

        seqs_w.up[mt->id] = 1;
        appRenewTempPriority(wk->position_z);
    }
}

/** @brief Transform and render a multi-texture object with pattern caching.
 *
 * ⚡ Opt4: Uses pre-built CG tile descriptors to skip the per-frame tile map walk.
 * The accumulated X/Y positions, TEX dimensions, and tile sizes are read from
 * the CGTileDesc cache instead of being re-parsed from TileMapEntry[].
 */
void mlt_obj_trans(MultiTexture* mt, WORK* wk, s32 base_y) {
    s32 rnum;
    s32 attr;
    s32 palo;
    s32 code;
    PatternCode cc;

    ppgSetupCurrentDataList(&mt->texList);

    if (mt->ext) {
        mlt_obj_trans_ext(mt, wk, base_y);
        return;
    }

    // ⚡ Opt4: Look up pre-built tile descriptors for this CG number
    CGTileCacheEntry* cge = cg_lookup_tile_descs(wk->cg_number);
    if (cge == NULL || cge->count == 0) {
        return;
    }

    attr = flptbl[wk->cg_flip ^ wk->rl_flag];
    palo = wk->colcd;

    if (wk->my_bright_type) {
        curr_bright = bright_type[(wk->my_bright_type - 1 < 4) ? wk->my_bright_type - 1 : 3]
                                 [(wk->my_bright_level < 16) ? wk->my_bright_level : 15];
    } else {
        curr_bright = 0xFFFFFF;
    }

    mlt_obj_matrix(wk, base_y);
    cc.parts.group = cge->group;

    // ⚡ Opt4: Iterate pre-built tile descriptors instead of walking TileMapEntry[]
    for (s32 t = 0; t < cge->count; t++) {
        const CGTileDesc* d = &cge->tiles[t];

        // Apply flip to cached cumulative offsets
        f32 x = (attr & 0x8000) ?  d->cum_x : -d->cum_x;
        f32 y = (attr & 0x4000) ? -d->cum_y :  d->cum_y;

        cc.parts.offset = d->tile_code;

        switch (d->wh) {
        case 1:
        case 2:
            if (get_mltbuf16(mt, cc.code, 0, &code) != 0) {
                lz77_gpu_or_cpu(d->tex_data, d->size * 2, d->size, mt, mt->mltgidx16, code, palo, 0);
            }

            if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                DebugLine(x - (d->dw & ((s16)attr >> 0x10)), y + (d->dh & ((s16)(attr * 2) >> 16)), d->dw, d->dh);
            }

            rnum = seqsStoreChip(x - (d->dw * BOOL(attr & 0x8000)),
                                 y + (d->dh * BOOL(attr & 0x4000)),
                                 d->dw,
                                 d->dh,
                                 mt->mltgidx16,
                                 code,
                                 palo | ((d->attr ^ attr) & 0xC000),
                                 wk->my_clear_level,
                                 mt->id);
            break;

        case 4:
            if (get_mltbuf32(mt, cc.code, 0, &code) != 0) {
                lz77_gpu_or_cpu(d->tex_data, d->size * 2, d->size, mt, mt->mltgidx32, code, palo, 1);
            }

            if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                DebugLine(x - (d->dw & ((s16)attr >> 0x10)), y + (d->dh & ((s16)(attr * 2) >> 16)), d->dw, d->dh);
            }

            rnum = seqsStoreChip(x - (d->dw * BOOL(attr & 0x8000)),
                                 y + (d->dh * BOOL(attr & 0x4000)),
                                 d->dw,
                                 d->dh,
                                 mt->mltgidx32,
                                 code,
                                 palo | (((d->attr ^ attr) & 0xC000) | 0x2000),
                                 wk->my_clear_level,
                                 mt->id);
            break;
        }

        if (rnum == 0) {
            break;
        }
    }

    seqs_w.up[mt->id] = 1;
    appRenewTempPriority(wk->position_z);
}


/** @brief Transform and render with CP3 palette (extended variant). */
void mlt_obj_trans_cp3_ext(MultiTexture* mt, WORK* wk, s32 base_y) {
    u32* textbl;
    u16* trsbas;
    TileMapEntry* trsptr;
    TEX* texptr;
    s32 rnum;
    s32 flip;
    s32 palo;
    s32 count;
    s32 n;
    s32 i;
    f32 x;
    f32 y;
    s16 ix;
    PatternCode cc;
    PatternInstance* cp;

    n = wk->cg_number;
    i = obj_group_table[n];

    if (i == 0) {
        return;
    }

    if (texgrplds[i].ok == 0) {
        // The trans data is not valid. Group number: %d\n
        flLogOut("トランスデータが有効ではありません。グループ番号：%d\n", i);
        return;
    }

    n -= texgrpdat[i].num_of_1st;
    trsbas = (u16*)(texgrplds[i].trans_table + ((u32*)texgrplds[i].trans_table)[n]);
    textbl = (u32*)texgrplds[i].texture_table;
    count = *trsbas;
    trsbas++;
    trsptr = (TileMapEntry*)trsbas;
    x = y = 0.0f;
    flip = flptbl[wk->cg_flip ^ wk->rl_flag];
    palo = wk->colcd;

    if (wk->my_bright_type) {
        curr_bright = bright_type[(wk->my_bright_type - 1 < 4) ? wk->my_bright_type - 1 : 3]
                                 [(wk->my_bright_level < 16) ? wk->my_bright_level : 15];
    } else {
        curr_bright = 0xFFFFFF;
    }

    mlt_obj_matrix(wk, base_y);
    cc.parts.group = 0;
    cc.parts.offset = wk->cg_number;
    ix = check_patcash_ex_trans(mt->cpat, cc.code);

    if (ix < 0) {
        {
            s32 size;
            s32 code;
            s32 wh;
            s32 dw;
            s32 dh;
            s32 attr;
            s32 palt;

            (void)dw;
            (void)dh;

            ix = get_free_patcash_index(mt->cpat);
            cp = &mt->cpat->patt[ix];
            mt->cpat->adr[mt->cpat->kazu] = cp;
            mt->cpat->kazu += 1;
            cp->curr_disp = 1;
            cp->time = mt->mltcshtime16;
            cp->cg.code = cc.code;
            cp->x16 = 0;
            cp->x32 = 0;
            SDL_zero(cp->map);
            cc.parts.group = i;

            while (count--) {
                if (flip & 0x8000) {
                    x += trsptr->x;
                } else {
                    x -= trsptr->x;
                }

                if (flip & 0x4000) {
                    y -= trsptr->y;
                } else {
                    y += trsptr->y;
                }

                texptr = (TEX*)((uintptr_t)textbl + ((u32*)textbl)[trsptr->code]);
                dw = (texptr->wh & 0xE0) >> 2;
                dh = (texptr->wh & 0x1C) * 2;
                wh = (texptr->wh & 3) + 1;
                size = (wh * wh) << 6;
                attr = trsptr->attr;
                palt = (attr & 0x1FF) + palo;
                attr = (attr ^ flip) & 0xC000;
                cc.parts.offset = trsptr->code;

                switch (wh) {
                case 1:
                case 2:
                    if (get_mltbuf16_ext_2(mt, cc.code, 0, &code, cp) != 0) {
                        lz77_gpu_or_cpu(&((u8*)texptr)[1], size * 2, size, mt, mt->mltgidx16, code, palt, 0);
                    }

                    if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                        DebugLine(x - (dw & ((s16)flip >> 0x10)), y + (dh & ((s16)(flip * 2) >> 16)), dw, dh);
                    }

                    rnum = seqsStoreChip(x - (dw * BOOL(flip & 0x8000)),
                                         y + (dh * BOOL(flip & 0x4000)),
                                         dw,
                                         dh,
                                         mt->mltgidx16,
                                         code,
                                         attr | palt,
                                         wk->my_clear_level,
                                         mt->id);
                    break;

                case 4:
                    if (get_mltbuf32_ext_2(mt, cc.code, 0, &code, cp) != 0) {
                        lz77_gpu_or_cpu(&((u8*)texptr)[1], size * 2, size, mt, mt->mltgidx32, code, palt, 1);
                    }

                    if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                        DebugLine(x - (dw & ((s16)flip >> 0x10)), y + (dh & ((s16)(flip * 2) >> 16)), dw, dh);
                    }

                    rnum = seqsStoreChip(x - (dw * BOOL(flip & 0x8000)),
                                         y + (dh * BOOL(flip & 0x4000)),
                                         dw,
                                         dh,
                                         mt->mltgidx32,
                                         code,
                                         (attr | 0x2000) | palt,
                                         wk->my_clear_level,
                                         mt->id);
                    break;
                }

                if (rnum == 0) {
                    break;
                }

                trsptr++;
            }

            seqs_w.up[mt->id] = 1;
            appRenewTempPriority(wk->position_z);
        }

        return;
    }

    {
        s32 code;
        s32 size;
        s32 wh;
        s32 dw;
        s32 dh;
        s32 attr;
        s32 palt;

        (void)dw;
        (void)dh;

        cp = mt->cpat->adr[ix];
        cp->curr_disp = 1;
        cp->time = mt->mltcshtime16;
        // makeup_tpu_free(mt->mltnum16 / 256, mt->mltnum32 / 64, &cp->map);
        cc.parts.group = i;

        while (count--) {
            if (flip & 0x8000) {
                x += trsptr->x;
            } else {
                x -= trsptr->x;
            }

            if (flip & 0x4000) {
                y -= trsptr->y;
            } else {
                y += trsptr->y;
            }

            texptr = (TEX*)((uintptr_t)textbl + ((u32*)textbl)[trsptr->code]);
            dw = (texptr->wh & 0xE0) >> 2;
            dh = (texptr->wh & 0x1C) * 2;
            wh = (texptr->wh & 3) + 1;
            size = (wh * wh) << 6;
            attr = trsptr->attr;
            palt = (attr & 0x1FF) + palo;
            attr = (attr ^ flip) & 0xC000;
            cc.parts.offset = trsptr->code;

            switch (wh) {
            case 1:
            case 2:
                if (get_mltbuf16_ext_2(mt, cc.code, 0, &code, cp) != 0) {
                    lz77_gpu_or_cpu(&((u8*)texptr)[1], size * 2, size, mt, mt->mltgidx16, code, palt, 0);
                }

                if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                    DebugLine(x - (dw & ((s16)flip >> 0x10)), y + (dh & ((s16)(flip * 2) >> 16)), dw, dh);
                }

                rnum = seqsStoreChip(x - (dw * BOOL(flip & 0x8000)),
                                     y + (dh * BOOL(flip & 0x4000)),
                                     dw,
                                     dh,
                                     mt->mltgidx16,
                                     code,
                                     attr | palt,
                                     wk->my_clear_level,
                                     mt->id);
                break;

            case 4:
                if (get_mltbuf32_ext_2(mt, cc.code, 0, &code, cp) != 0) {
                    lz77_gpu_or_cpu(&((u8*)texptr)[1], size * 2, size, mt, mt->mltgidx32, code, palt, 1);
                }

                if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                    DebugLine(x - (dw & ((s16)flip >> 0x10)), y + (dh & ((s16)(flip * 2) >> 16)), dw, dh);
                }

                rnum = seqsStoreChip(x - (dw * BOOL(flip & 0x8000)),
                                     y + (dh * BOOL(flip & 0x4000)),
                                     dw,
                                     dh,
                                     mt->mltgidx32,
                                     code,
                                     (attr | 0x2000) | palt,
                                     wk->my_clear_level,
                                     mt->id);
                break;
            }

            if (rnum == 0) {
                break;
            }

            trsptr++;
        }

        seqs_w.up[mt->id] = 1;
        appRenewTempPriority(wk->position_z);
    }
}

/** @brief Transform and render with CP3 palette mapping.
 *
 * `+"`u{26A1}`+" Opt4: Uses pre-built CG tile descriptors to skip the per-frame tile map walk.
 */
void mlt_obj_trans_cp3(MultiTexture* mt, WORK* wk, s32 base_y) {
    s32 rnum;
    s32 flip;
    s32 palo;
    s32 code;
    s32 attr;
    s32 palt;
    PatternCode cc;

    ppgSetupCurrentDataList(&mt->texList);

    if (mt->ext) {
        mlt_obj_trans_cp3_ext(mt, wk, base_y);
        return;
    }

    // `+"`u{26A1}`+" Opt4: Look up pre-built tile descriptors for this CG number
    CGTileCacheEntry* cge = cg_lookup_tile_descs(wk->cg_number);
    if (cge == NULL || cge->count == 0) {
        return;
    }

    flip = flptbl[wk->cg_flip ^ wk->rl_flag];
    palo = wk->colcd;

    if (wk->my_bright_type) {
        curr_bright = bright_type[(wk->my_bright_type - 1 < 4) ? wk->my_bright_type - 1 : 3]
                                 [(wk->my_bright_level < 16) ? wk->my_bright_level : 15];
    } else {
        curr_bright = 0xFFFFFF;
    }

    mlt_obj_matrix(wk, base_y);
    cc.parts.group = cge->group;

    // `+"`u{26A1}`+" Opt4: Iterate pre-built tile descriptors instead of walking TileMapEntry[]
    for (s32 t = 0; t < cge->count; t++) {
        const CGTileDesc* d = &cge->tiles[t];

        // Apply flip to cached cumulative offsets
        f32 x = (flip & 0x8000) ?  d->cum_x : -d->cum_x;
        f32 y = (flip & 0x4000) ? -d->cum_y :  d->cum_y;

        attr = d->attr;
        palt = (attr & 0x1FF) + palo;
        attr = (attr ^ flip) & 0xC000;
        cc.parts.offset = d->tile_code;

        switch (d->wh) {
        case 1:
        case 2:
            if (get_mltbuf16(mt, cc.code, 0, &code) != 0) {
                lz77_gpu_or_cpu(d->tex_data, d->size * 2, d->size, mt, mt->mltgidx16, code, palt, 0);
            }

            if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                DebugLine(x - (d->dw & ((s16)flip >> 0x10)), y + (d->dh & ((s16)(flip * 2) >> 16)), d->dw, d->dh);
            }

            rnum = seqsStoreChip(x - (d->dw * BOOL(flip & 0x8000)),
                                 y + (d->dh * BOOL(flip & 0x4000)),
                                 d->dw,
                                 d->dh,
                                 mt->mltgidx16,
                                 code,
                                 attr | palt,
                                 wk->my_clear_level,
                                 mt->id);
            break;

        case 4:
            if (get_mltbuf32(mt, cc.code, 0, &code) != 0) {
                lz77_gpu_or_cpu(d->tex_data, d->size * 2, d->size, mt, mt->mltgidx32, code, palt, 1);
            }

            if (Debug_w[DEBUG_OBJ_SIZE_LINE]) {
                DebugLine(x - (d->dw & ((s16)flip >> 0x10)), y + (d->dh & ((s16)(flip * 2) >> 16)), d->dw, d->dh);
            }

            rnum = seqsStoreChip(x - (d->dw * BOOL(flip & 0x8000)),
                                 y + (d->dh * BOOL(flip & 0x4000)),
                                 d->dw,
                                 d->dh,
                                 mt->mltgidx32,
                                 code,
                                 attr | 0x2000 | palt,
                                 wk->my_clear_level,
                                 mt->id);
            break;
        }

        if (rnum == 0) {
            break;
        }
    }

    seqs_w.up[mt->id] = 1;
    appRenewTempPriority(wk->position_z);
}

/** @brief Transform and render in RGB mode (extended variant). */
void mlt_obj_trans_rgb_ext(MultiTexture* mt, WORK* wk, s32 base_y) {
    u32* textbl;
    u16* trsbas;
    TileMapEntry* trsptr;
    TEX* texptr;
    s32 rnum;
    s32 flip;
    s32 palo;
    s32 count;
    s32 n;
    s32 i;
    f32 x;
    f32 y;
    s16 ix;
    PatternCode cc;
    PatternInstance* cp;

    (void)textbl;

    n = wk->cg_number;
    i = obj_group_table[n];

    if (i == 0) {
        return;
    }

    if (texgrplds[i].ok == 0) {
        // The trans data is not valid. Group number: %d\n
        flLogOut("トランスデータが有効ではありません。グループ番号：%d\n", i);
        return;
    }

    n -= texgrpdat[i].num_of_1st;
    trsbas = (u16*)(texgrplds[i].trans_table + ((u32*)texgrplds[i].trans_table)[n]);
    textbl = (u32*)texgrplds[i].texture_table;
    count = *trsbas;
    trsbas++;
    trsptr = (TileMapEntry*)trsbas;
    x = y = 0.0f;
    flip = flptbl[wk->cg_flip ^ wk->rl_flag];
    palo = wk->colcd;

    if (wk->my_bright_type) {
        curr_bright = bright_type[(wk->my_bright_type - 1 < 4) ? wk->my_bright_type - 1 : 3]
                                 [(wk->my_bright_level < 16) ? wk->my_bright_level : 15];
    } else {
        curr_bright = 0xFFFFFF;
    }

    mlt_obj_matrix(wk, base_y);
    cc.parts.group = wk->colcd;
    cc.parts.offset = wk->cg_number;
    ix = check_patcash_ex_trans(mt->cpat, cc.code);

    if (ix < 0) {
        {
            s32 size;
            s32 code;
            s32 attr;
            s32 palt;
            s32 wh;
            s32 dw;
            s32 dh;

            ix = get_free_patcash_index(mt->cpat);
            cp = &mt->cpat->patt[ix];
            mt->cpat->adr[mt->cpat->kazu] = cp;
            mt->cpat->kazu += 1;
            cp->curr_disp = 1;
            cp->time = mt->mltcshtime16;
            cp->cg.code = cc.code;
            cp->x16 = 0;
            cp->x32 = 0;
            SDL_zero(cp->map);
            cc.parts.group = i;

            while (count--) {
                if (flip & 0x8000) {
                    x += trsptr->x;
                } else {
                    x -= trsptr->x;
                }

                if (flip & 0x4000) {
                    y -= trsptr->y;
                } else {
                    y += trsptr->y;
                }

                texptr = (TEX*)((uintptr_t)textbl + ((u32*)textbl)[trsptr->code]);
                dw = (texptr->wh & 0xE0) >> 2;
                dh = (texptr->wh & 0x1C) * 2;
                wh = (texptr->wh & 3) + 1;
                size = (wh * wh) << 6;
                attr = trsptr->attr;
                palt = (attr & 0x1FF) + palo;
                attr = (attr ^ flip) & 0xC000;
                cc.parts.offset = trsptr->code;

                switch (wh) {
                case 1:
                case 2:
                    if (get_mltbuf16_ext_2(mt, cc.code, palt, &code, cp) != 0) {
                        lz_ext_p6_cx(&((u8*)texptr)[1], (u16*)mt->mltbuf, size, (u16*)(ColorRAM[palt]));
                        Renderer_UpdateTexture(mt->mltgidx16 + (code >> 8), mt->mltbuf, code & 0xFF, size * 2, 0, 0);
                    }

                    rnum = seqsStoreChip(x - (dw * BOOL(flip & 0x8000)),
                                         y + (dh * BOOL(flip & 0x4000)),
                                         dw,
                                         dh,
                                         mt->mltgidx16,
                                         code,
                                         attr,
                                         wk->my_clear_level,
                                         mt->id);
                    break;

                case 4:
                    if (get_mltbuf32_ext_2(mt, cc.code, palt, &code, cp) != 0) {
                        lz_ext_p6_cx(&((u8*)texptr)[1], (u16*)mt->mltbuf, size, (u16*)(ColorRAM[palt]));
                        Renderer_UpdateTexture(mt->mltgidx32 + (code >> 6), mt->mltbuf, code & 0x3F, size * 2, 0, 0);
                    }

                    rnum = seqsStoreChip(x - (dw * BOOL(flip & 0x8000)),
                                         y + (dh * BOOL(flip & 0x4000)),
                                         dw,
                                         dh,
                                         mt->mltgidx32,
                                         code,
                                         attr | 0x2000,
                                         wk->my_clear_level,
                                         mt->id);
                    break;
                }

                if (rnum == 0) {
                    break;
                }

                trsptr++;
            }

            seqs_w.up[mt->id] = 1;
            appRenewTempPriority(wk->position_z);
        }

        return;
    }

    {
        s32 code;
        s32 size;
        s32 attr;
        s32 palt;
        s32 wh;
        s32 dw;
        s32 dh;

        cp = mt->cpat->adr[ix];
        cp->curr_disp = 1;
        cp->time = mt->mltcshtime16;
        // makeup_tpu_free(mt->mltnum16 / 256, mt->mltnum32 / 64, &cp->map);
        cc.parts.group = i;

        while (count--) {
            if (flip & 0x8000) {
                x += trsptr->x;
            } else {
                x -= trsptr->x;
            }

            if (flip & 0x4000) {
                y -= trsptr->y;
            } else {
                y += trsptr->y;
            }

            texptr = (TEX*)((uintptr_t)textbl + ((u32*)textbl)[trsptr->code]);
            dw = (texptr->wh & 0xE0) >> 2;
            dh = (texptr->wh & 0x1C) * 2;
            wh = (texptr->wh & 3) + 1;
            size = (wh * wh) << 6;
            attr = trsptr->attr;
            palt = (attr & 0x1FF) + palo;
            attr = (attr ^ flip) & 0xC000;
            cc.parts.offset = trsptr->code;

            switch (wh) {
            case 1:
            case 2:
                if (get_mltbuf16_ext_2(mt, cc.code, palt, &code, cp) != 0) {
                    lz_ext_p6_cx(&((u8*)texptr)[1], (u16*)mt->mltbuf, size, (u16*)(ColorRAM[palt]));
                    Renderer_UpdateTexture(mt->mltgidx16 + (code >> 8), mt->mltbuf, code & 0xFF, size * 2, 0, 0);
                }

                rnum = seqsStoreChip(x - (dw * BOOL(flip & 0x8000)),
                                     y + (dh * BOOL(flip & 0x4000)),
                                     dw,
                                     dh,
                                     mt->mltgidx16,
                                     code,
                                     attr,
                                     wk->my_clear_level,
                                     mt->id);
                break;

            case 4:
                if (get_mltbuf32_ext_2(mt, cc.code, palt, &code, cp) != 0) {
                    lz_ext_p6_cx(&((u8*)texptr)[1], (u16*)mt->mltbuf, size, (u16*)(ColorRAM[palt]));
                    Renderer_UpdateTexture(mt->mltgidx32 + (code >> 6), mt->mltbuf, code & 0x3F, size * 2, 0, 0);
                }

                rnum = seqsStoreChip(x - (dw * BOOL(flip & 0x8000)),
                                     y + (dh * BOOL(flip & 0x4000)),
                                     dw,
                                     dh,
                                     mt->mltgidx32,
                                     code,
                                     attr | 0x2000,
                                     wk->my_clear_level,
                                     mt->id);
                break;
            }

            if (rnum == 0) {
                break;
            }

            trsptr++;
        }

        seqs_w.up[mt->id] = 1;
        appRenewTempPriority(wk->position_z);
    }
}

/** @brief Transform and render in RGB mode with pattern caching.
 *
 * `+"`u{26A1}`+" Opt4: Uses pre-built CG tile descriptors to skip the per-frame tile map walk.
 */
void mlt_obj_trans_rgb(MultiTexture* mt, WORK* wk, s32 base_y) {
    s32 rnum;
    s32 flip;
    s32 palo;
    s32 code;
    s32 attr;
    s32 palt;
    PatternCode cc;

    ppgSetupCurrentDataList(&mt->texList);

    if (mt->ext) {
        mlt_obj_trans_rgb_ext(mt, wk, base_y);
        return;
    }

    // `+"`u{26A1}`+" Opt4: Look up pre-built tile descriptors for this CG number
    CGTileCacheEntry* cge = cg_lookup_tile_descs(wk->cg_number);
    if (cge == NULL || cge->count == 0) {
        return;
    }

    flip = flptbl[wk->cg_flip ^ wk->rl_flag];
    palo = wk->colcd;

    if (wk->my_bright_type) {
        curr_bright = bright_type[(wk->my_bright_type - 1 < 4) ? wk->my_bright_type - 1 : 3]
                                 [(wk->my_bright_level < 16) ? wk->my_bright_level : 15];
    } else {
        curr_bright = 0xFFFFFF;
    }

    mlt_obj_matrix(wk, base_y);
    cc.parts.group = cge->group;

    // `+"`u{26A1}`+" Opt4: Iterate pre-built tile descriptors instead of walking TileMapEntry[]
    for (s32 t = 0; t < cge->count; t++) {
        const CGTileDesc* d = &cge->tiles[t];

        // Apply flip to cached cumulative offsets
        f32 x = (flip & 0x8000) ?  d->cum_x : -d->cum_x;
        f32 y = (flip & 0x4000) ? -d->cum_y :  d->cum_y;

        attr = d->attr;
        palt = (attr & 0x1FF) + palo;
        attr = (attr ^ flip) & 0xC000;
        cc.parts.offset = d->tile_code;

        switch (d->wh) {
        case 1:
        case 2:
            if (get_mltbuf16(mt, cc.code, palt, &code) != 0) {
                lz_ext_p6_cx(d->tex_data, (u16*)mt->mltbuf, d->size, (u16*)(ColorRAM[palt]));
                Renderer_UpdateTexture(mt->mltgidx16 + (code >> 8), mt->mltbuf, code & 0xFF, d->size * 2, 0, 0);
            }

            rnum = seqsStoreChip(x - (d->dw * BOOL(flip & 0x8000)),
                                 y + (d->dh * BOOL(flip & 0x4000)),
                                 d->dw,
                                 d->dh,
                                 mt->mltgidx16,
                                 code,
                                 attr,
                                 wk->my_clear_level,
                                 mt->id);
            break;

        case 4:
            if (get_mltbuf32(mt, cc.code, palt, &code) != 0) {
                lz_ext_p6_cx(d->tex_data, (u16*)mt->mltbuf, d->size, (u16*)(ColorRAM[palt]));
                Renderer_UpdateTexture(mt->mltgidx32 + (code >> 6), mt->mltbuf, code & 0x3F, d->size * 2, 0, 0);
            }

            rnum = seqsStoreChip(x - (d->dw * BOOL(flip & 0x8000)),
                                 y + (d->dh * BOOL(flip & 0x4000)),
                                 d->dw,
                                 d->dh,
                                 mt->mltgidx32,
                                 code,
                                 attr | 0x2000,
                                 wk->my_clear_level,
                                 mt->id);
            break;
        }

        if (rnum == 0) {
            break;
        }
    }

    seqs_w.up[mt->id] = 1;
    appRenewTempPriority(wk->position_z);
}

/** @brief Set up the transformation matrix for a multi-texture object.
 *
 * ⚡ Opt3: After building the matrix, cache its elements into statics
 * for inlined per-chip transforms in seqsStoreChip().
 */
void mlt_obj_matrix(WORK* wk, s32 base_y) {
    njSetMatrix(NULL, &BgMATRIX[wk->my_family]);
    njTranslate(NULL, wk->position_x, wk->position_y + base_y, PrioBase[wk->position_z]);

    if (wk->my_mr_flag) {
        njScale(NULL, (1.0f / 64.0f) * (wk->my_mr.size.x + 1), (1.0f / 64.0f) * (wk->my_mr.size.y + 1), 1.0f);
    }

    // ⚡ Opt3: Cache matrix elements for inlined transform.
    // Since chip input z==0, we only need rows 0,1,3 (skip row 2 multiply).
    // row 2 only matters for Z-step via appRenewTempPriority_1_Chip.
    MTX cached;
    njGetMatrix(&cached);
    s_mtx_00 = cached.a[0][0]; s_mtx_01 = cached.a[0][1]; s_mtx_02 = cached.a[0][2];
    s_mtx_10 = cached.a[1][0]; s_mtx_11 = cached.a[1][1]; s_mtx_12 = cached.a[1][2];
    s_mtx_tx = cached.a[3][0]; s_mtx_ty = cached.a[3][1]; s_mtx_tz = cached.a[3][2];
    s_mtx_z_step = cached.a[2][2] * (1.0f / 65536.0f);
}

/** @brief Initialize the base sprite priority table. */
void appSetupBasePriority() {
    s32 i;

    for (i = 0; i < PRIO_BASE_SIZE; i++) {
        PrioBaseOriginal[i] = ((i * 512) + 1) / 65535.0f;
    }
}

/** @brief Reset the temporary sprite priority table from the base. */
void appSetupTempPriority() {
    // ⚡ Bolt: bulk memcpy replaces per-element loop (128 floats = 512 bytes per frame)
    memcpy(PrioBase, PrioBaseOriginal, sizeof(PrioBase));
}

/** @brief Reserved: renew temporary priority for a single chip.
 *
 * ⚡ Opt3: Inlines the Z bump for the cached matrix elements,
 * keeping both the real cmtx and the cached statics in sync.
 */
void appRenewTempPriority_1_Chip() {
    // ⚡ Bolt: Z-only translate avoids full 4×4 matmul (~130 FLOPs → 8 FLOPs)
    njTranslateZ(1.0f / 65536.0f);
    // ⚡ Opt3: Also bump the cached Z translation for inlined transforms
    s_mtx_tz += s_mtx_z_step;
}

/** @brief Renew the temporary priority based on the given Z position. */
static void appRenewTempPriority(s32 z) {
    MTX mtx;
    njGetMatrix(&mtx);
    PrioBase[z] = mtx.a[3][2];
}

/** @brief Initialize the sprite chip set system with the given memory.
 *
 * ⚡ Opt8: Allocates two buffers for double-buffering.
 */
void seqsInitialize(void* adrs) {
    if (adrs == NULL) {
        flLogOut("[mtrans] seqsInitialize: NULL address");
        return;
    }

    // ⚡ Opt8: Split the allocated memory into two halves for double-buffering.
    // Total memory is 0xD000 bytes — each buffer gets half.
    const u32 half = seqsGetUseMemorySize() / 2;
    seqs_w.chip_buf0 = (Sprite2*)adrs;
    seqs_w.chip_buf1 = (Sprite2*)((u8*)adrs + half);
    seqs_w.chip = seqs_w.chip_buf0;
    seqs_w.buf_index = 0;
    seqs_w.sprMax = 0;
}

/** @brief Get the maximum number of sprites allowed. */
u16 seqsGetSprMax() {
    return seqs_w.sprMax;
}

/** @brief Get the memory usage of the sprite chip set system.
 *
 * ⚡ Opt8: Doubled to accommodate two buffers.
 */
u32 seqsGetUseMemorySize() {
    return 0xD000 * 2;
}

/** @brief Pre-frame reset of the sprite chip set system.
 *
 * ⚡ Opt8: Swaps to the alternate buffer each frame so the GPU
 * may still read from the previous frame's data.
 */
void seqsBeforeProcess() {
    TRACE_PLOT_INT("Sprites", seqs_w.sprTotal);
    seqs_w.sprTotal = 0;
    // ⚡ Bolt: bulk memset replaces per-element loop (24 bytes per frame)
    memset(seqs_w.up, 0, sizeof(seqs_w.up));

    // ⚡ Opt8: Swap to the other buffer
    seqs_w.buf_index ^= 1;
    seqs_w.chip = seqs_w.buf_index ? seqs_w.chip_buf1 : seqs_w.chip_buf0;
}

/** @brief Post-frame processing: flush accumulated sprites for rendering.
 *
 * ⚡ Bolt: Phase 2 uses the batch flush API which:
 *  - GPU backend: sorts by tex_code, inlines SetTexture + vertex writes
 *  - GL backend: simple per-sprite loop (preserves Z-order)
 */
void seqsAfterProcess() {
    s32 i;

    if ((Debug_w[DEBUG_NO_DISP_TYPE_SB] != 3) && (seqs_w.sprTotal != 0)) {
        // Phase 1: Upload dirty texture data
        for (i = 0; i < SPRITE_LAYERS_MAX; i++) {
            if (seqs_w.up[i]) {
                if (Debug_w[DEBUG_NO_UPDATE_TEXCASH]) {
                    if (ppgCheckTextureDataBe(mts[i].texList.tex) == 0) {
                        seqs_w.up[i] = 0;
                    }
                } else if (ppgRenewTexChunkSeqs(mts[i].texList.tex) == 0) {
                    seqs_w.up[i] = 0;
                }
            }
        }

        if (seqs_w.sprMax < seqs_w.sprTotal) {
            seqs_w.sprMax = seqs_w.sprTotal;
        }

        // Phase 2: ⚡ Batch flush all sprites in one call
        SDLGameRenderer_FlushSprite2Batch(seqs_w.chip, seqs_w.up, seqs_w.sprTotal);
    }
}

/** @brief Store a sprite chip into the chip set for deferred rendering.
 *
 * ⚡ Opt3: Replaces njCalcPoints() call with inlined affine transform using
 * pre-cached matrix elements from mlt_obj_matrix(). Since input z==0 for all
 * chips, the z-row multiply is eliminated entirely. Per-chip Z increment is
 * also inlined — just a float add instead of the njTranslateZ → 4-float update.
 *
 * Savings per chip: eliminates function call chain (seqsStoreChip → njCalcPoints
 * → njCalcPoint × 2) with its NULL checks, loop overhead, and z-row multiplies.
 * Net: ~20 FLOPs + overhead → ~12 FLOPs flat.
 */
static s32 seqsStoreChip(f32 x, f32 y, s32 w, s32 h, s32 gix, s32 code, s32 attr, s32 alpha, s32 id) {
    Sprite2* chip;
    s32 u;
    s32 v;

    const f32 dx = 0;
    const f32 dy = 0;

    chip = &seqs_w.chip[seqs_w.sprTotal];

    // ⚡ Opt5: SIMD affine transform — process both points in parallel.
    // Layout: vec = {x0, x1, -, -} for each component multiply.
    // Uses FMA (fused multiply-add) for better precision and throughput.
    {
        const f32 x1 = x + w;
        const f32 y1 = y - h;

        // Pack both points' x and y coords into SIMD lanes: [x0, x1, x0, x1]
        const simde__m128 vx = simde_mm_set_ps(x1, x, x1, x);
        const simde__m128 vy = simde_mm_set_ps(y1, y, y1, y);

        // Matrix elements broadcast: [m00, m00, m01, m01], etc.
        const simde__m128 vm0 = simde_mm_set_ps(s_mtx_01, s_mtx_01, s_mtx_00, s_mtx_00);
        const simde__m128 vm1 = simde_mm_set_ps(s_mtx_11, s_mtx_11, s_mtx_10, s_mtx_10);
        const simde__m128 vt  = simde_mm_set_ps(s_mtx_ty, s_mtx_ty, s_mtx_tx, s_mtx_tx);

        // result = vx * vm0 + vy * vm1 + vt  (X0, X1, Y0, Y1)
        simde__m128 result = simde_mm_fmadd_ps(vx, vm0, vt);
        result = simde_mm_fmadd_ps(vy, vm1, result);

        // Extract results
        simde_float32 SIMDE_VECTOR(16) r;
        simde_mm_store_ps((simde_float32*)&r, result);
        chip->v[0].x = ((simde_float32*)&r)[0];
        chip->v[1].x = ((simde_float32*)&r)[1];
        chip->v[0].y = ((simde_float32*)&r)[2];
        chip->v[1].y = ((simde_float32*)&r)[3];

        // Z is cheap enough to stay scalar (2 FMAs)
        chip->v[0].z = x  * s_mtx_02 + y  * s_mtx_12 + s_mtx_tz;
        chip->v[1].z = x1 * s_mtx_02 + y1 * s_mtx_12 + s_mtx_tz;
    }

    if ((chip->v[0].x >= 384.0f) || (chip->v[1].x < 0.0f) || (chip->v[0].y >= 224.0f) || (chip->v[1].y < 0.0f)) {
        return 1;
    }

    if (!(attr & 0x2000)) {
        u = (code & 0xF) * 16;
        v = code & 0xF0;
        chip->tex_code = ppgGetUsingTextureHandle(NULL, gix + (code >> 8));
    } else {
        u = (code & 7) * 32;
        v = (code & 0x38) * 4;
        chip->tex_code = ppgGetUsingTextureHandle(NULL, gix + (code >> 6));
    }

    // ⚡ Opt3: Inlined Z bump — replaces appRenewTempPriority_1_Chip() → njTranslateZ.
    // Update both the real cmtx (for non-seqsStoreChip callers like DebugLine, appRenewTempPriority)
    // and the cached statics.
    njTranslateZ(1.0f / 65536.0f);
    s_mtx_tz += s_mtx_z_step;

    if (attr & 0x8000) {
        chip->t[1].s = (u - dx) / 256.0f;
        chip->t[0].s = (u + w - dx) / 256.0f;
    } else {
        chip->t[0].s = (u + dx) / 256.0f;
        chip->t[1].s = (u + w + dx) / 256.0f;
    }

    if (attr & 0x4000) {
        chip->t[1].t = (v - dy) / 256.0f;
        chip->t[0].t = (v + h - dy) / 256.0f;
    } else {
        chip->t[0].t = (v + dy) / 256.0f;
        chip->t[1].t = (v + h + dy) / 256.0f;
    }

    chip->tex_code |= ppgGetUsingPaletteHandle(NULL, attr & 0x1FF) << 16;
    chip->vertex_color = curr_bright | ((0xFF - alpha) << 24);
    chip->id = id;
    chip->modelX = 0.0f;
    chip->modelY = 0.0f;
    seqs_w.sprTotal += 1;

    if (seqs_w.sprTotal > 0x400) {
        // The number of OBJ fragments has exceeded the planned number
        flLogOut("ＯＢＪの破片が予定数を越えてしまいました");
        return 0;
    }

    return 1;
}

/** @brief Look up or allocate a 16×16 tile buffer slot.
 *  ⚡ Uses hash table for O(1) lookup instead of linear scan.
 *  ⚡ Opt2P2: Boost-on-hit + LRU eviction for persistent tile caching. */
static s32 get_mltbuf16(MultiTexture* mt, u32 code, u32 palt, s32* ret) {
    MltHashEntry* ht = s_hash16[mt->id];
    u32 h = mlt_hash(code);
    s32 probe;

    // Hash probe: look for existing entry
    for (probe = 0; probe < MLT_HASH_SIZE; probe++) {
        u32 idx = (h + probe) & MLT_HASH_MASK;
        if (ht[idx].slot == MLT_HASH_EMPTY) {
            break; // Empty slot — not in cache
        }
        if (ht[idx].code == code && mt->mltcsh16[ht[idx].slot].state == palt) {
            // ⚡ Opt2P2: Cache hit — boost lifetime so frequently-used tiles persist longer
            mt->mltcsh16[ht[idx].slot].time = mt->mltcshtime16 * TILE_CACHE_BOOST;
            *ret = ht[idx].slot;
            return 0;
        }
    }

    // Cache miss — find a free slot in the PatternState array
    s32 b = -1;
    PatternState* mc = mt->mltcsh16;
    for (s32 i = 0; i < mt->mltnum16; i++) {
        if (mc[i].cs.code == (u32)-1) {
            b = i;
            break;
        }
    }

    if (b < 0) {
        // ⚡ Opt2P2: LRU eviction — find the slot with the smallest time value
        s32 min_time = INT32_MAX;
        for (s32 k = 0; k < mt->mltnum16; k++) {
            if (mc[k].time < min_time) {
                min_time = mc[k].time;
                b = k;
            }
        }
        if (b >= 0) {
            // Invalidate the old hash entry before overwriting
            u32 old_code = mc[b].cs.code;
            u32 oh = mlt_hash(old_code);
            for (s32 p = 0; p < MLT_HASH_SIZE; p++) {
                u32 oi = (oh + p) & MLT_HASH_MASK;
                if (ht[oi].slot == MLT_HASH_EMPTY) break;
                if (ht[oi].code == old_code && ht[oi].slot == b) {
                    ht[oi].slot = MLT_HASH_EMPTY;
                    ht[oi].code = 0;
                    break;
                }
            }
        }
    }

    // Populate the cache slot
    mt->mltcsh16[b].time = mt->mltcshtime16;
    mt->mltcsh16[b].state = palt;
    mt->mltcsh16[b].cs.code = code;
    *ret = b;

    // Insert into hash table
    u32 ins = mlt_hash(code);
    for (probe = 0; probe < MLT_HASH_SIZE; probe++) {
        u32 idx = (ins + probe) & MLT_HASH_MASK;
        if (ht[idx].slot == MLT_HASH_EMPTY) {
            ht[idx].code = code;
            ht[idx].slot = b;
            break;
        }
    }

    return 1;
}

/** @brief Look up or allocate a 32×32 tile buffer slot.
 *  ⚡ Uses hash table for O(1) lookup instead of linear scan.
 *  ⚡ Opt2P2: Boost-on-hit + LRU eviction for persistent tile caching. */
static s32 get_mltbuf32(MultiTexture* mt, u32 code, u32 palt, s32* ret) {
    MltHashEntry* ht = s_hash32[mt->id];
    u32 h = mlt_hash(code);
    s32 probe;

    // Hash probe: look for existing entry
    for (probe = 0; probe < MLT_HASH_SIZE; probe++) {
        u32 idx = (h + probe) & MLT_HASH_MASK;
        if (ht[idx].slot == MLT_HASH_EMPTY) {
            break; // Empty slot — not in cache
        }
        if (ht[idx].code == code && mt->mltcsh32[ht[idx].slot].state == palt) {
            // ⚡ Opt2P2: Cache hit — boost lifetime so frequently-used tiles persist longer
            mt->mltcsh32[ht[idx].slot].time = mt->mltcshtime32 * TILE_CACHE_BOOST;
            *ret = ht[idx].slot;
            return 0;
        }
    }

    // Cache miss — find a free slot in the PatternState array
    s32 b = -1;
    PatternState* mc = mt->mltcsh32;
    for (s32 i = 0; i < mt->mltnum32; i++) {
        if (mc[i].cs.code == (u32)-1) {
            b = i;
            break;
        }
    }

    if (b < 0) {
        // ⚡ Opt2P2: LRU eviction — find the slot with the smallest time value
        s32 min_time = INT32_MAX;
        for (s32 k = 0; k < mt->mltnum32; k++) {
            if (mc[k].time < min_time) {
                min_time = mc[k].time;
                b = k;
            }
        }
        if (b >= 0) {
            // Invalidate the old hash entry before overwriting
            u32 old_code = mc[b].cs.code;
            u32 oh = mlt_hash(old_code);
            for (s32 p = 0; p < MLT_HASH_SIZE; p++) {
                u32 oi = (oh + p) & MLT_HASH_MASK;
                if (ht[oi].slot == MLT_HASH_EMPTY) break;
                if (ht[oi].code == old_code && ht[oi].slot == b) {
                    ht[oi].slot = MLT_HASH_EMPTY;
                    ht[oi].code = 0;
                    break;
                }
            }
        }
    }

    // Populate the cache slot
    mt->mltcsh32[b].time = mt->mltcshtime32;
    mt->mltcsh32[b].state = palt;
    mt->mltcsh32[b].cs.code = code;
    *ret = b;

    // Insert into hash table
    u32 ins = mlt_hash(code);
    for (probe = 0; probe < MLT_HASH_SIZE; probe++) {
        u32 idx = (ins + probe) & MLT_HASH_MASK;
        if (ht[idx].slot == MLT_HASH_EMPTY) {
            ht[idx].code = code;
            ht[idx].slot = b;
            break;
        }
    }

    return 1;
}

/** @brief Look up or allocate a 16×16 tile buffer slot (extended, with cache). */
static s32 get_mltbuf16_ext_2(MultiTexture* mt, u32 code, u32 palt, s32* ret, PatternInstance* cp) {
    PatternState* mc = mt->mltcsh16;
    s32 i;

    for (i = 0; i < mt->tpu->x16; i++) {
        if ((code == mc[mt->tpu->x16_used[i]].cs.code) && (palt == mc[mt->tpu->x16_used[i]].state)) {
            *ret = mt->tpu->x16_used[i];

            if (x16_mapping_set(&cp->map, *ret)) {
                cp->x16 += 1;
                mc[mt->tpu->x16_used[i]].time += 1;
            }

            return 0;
        }
    }

    if ((i != mt->mltnum16) && (mt->tpf->x16 != 0)) {
        mt->tpf->x16 -= 1;
        mt->tpu->x16_used[i] = mt->tpf->x16_free[mt->tpf->x16];
        mt->tpu->x16 += 1;
        mc[mt->tpu->x16_used[i]].cs.code = code;
        mc[mt->tpu->x16_used[i]].state = palt;
        *ret = mt->tpu->x16_used[i];
        mc[mt->tpu->x16_used[i]].time = 1;

        if (x16_mapping_set(&cp->map, *ret)) {
            cp->x16 += 1;
        }

        return 1;
    }

    // CG cache is full. x16 EXT2\n
    flLogOut("ＣＧキャッシュが一杯になりました。×１６　ＥＸＴ２\n");
    return 0;
}

/** @brief Look up or allocate a 32×32 tile buffer slot (extended, with cache). */
static s32 get_mltbuf32_ext_2(MultiTexture* mt, u32 code, u32 palt, s32* ret, PatternInstance* cp) {
    PatternState* mc = mt->mltcsh32;
    s32 i;

    for (i = 0; i < mt->tpu->x32; i++) {
        if ((code == mc[mt->tpu->x32_used[i]].cs.code) && (palt == mc[mt->tpu->x32_used[i]].state)) {
            *ret = mt->tpu->x32_used[i];

            if (x32_mapping_set(&cp->map, *ret)) {
                cp->x32 += 1;
                mc[mt->tpu->x32_used[i]].time += 1;
            }

            return 0;
        }
    }

    if ((i != mt->mltnum32) && (mt->tpf->x32 != 0)) {
        mt->tpf->x32 -= 1;
        mt->tpu->x32_used[i] = mt->tpf->x32_free[mt->tpf->x32];
        mt->tpu->x32 += 1;
        mc[mt->tpu->x32_used[i]].cs.code = code;
        mc[mt->tpu->x32_used[i]].state = palt;
        *ret = mt->tpu->x32_used[i];
        mc[mt->tpu->x32_used[i]].time += 1;

        if (x32_mapping_set(&cp->map, *ret)) {
            cp->x32 += 1;
        }

        return 1;
    }

    flLogOut("ＣＧキャッシュが一杯になりました。×３２　ＥＸＴ２\n");
    return 0;
}

/** @brief Set the 16×16 tile mapping for a given pattern code. */
static u16 x16_mapping_set(PatternMap* map, s32 code) {
    u16 num;
    u16 flg;

    flg = 0;
    num = code & 0xF;

    if (!((1 << (num)) & (map->x16_map[code / 256][(code % 256) / 16]))) {
        map->x16_map[code / 256][(code % 256) / 16] |= (1 << num);
        flg = 1;
    }

    return flg;
}

/** @brief Set the 32×32 tile mapping for a given pattern code. */
static u16 x32_mapping_set(PatternMap* map, s32 code) {
    u16 flg = 0;
    u8 num = code & 7;

    if (!((map->x32_map[code / 64][(code % 64) / 8]) & (1 << num))) {
        map->x32_map[code / 64][(code % 64) / 8] |= (1 << num);
        flg = 1;
    }

    return flg;
}

/** @brief Populate the free texture pool based on current pattern mappings. */
void makeup_tpu_free(s32 x16, s32 x32, PatternMap* map) {
    s16 i;
    s16 j;
    s16 k;

    tpu_free->x16 = 0;
    tpu_free->x32 = 0;

    for (i = 0; i < x16; i++) {
        for (j = 0; j < 16; j++) {
            if (map->x16_map[i][j] != 0) {
                for (k = 0; k < 16; k++) {
                    if ((1 << k) & map->x16_map[i][j]) {
                        tpu_free->x16_used[tpu_free->x16] = (i * 256) + (j * 16) + k;
                        tpu_free->x16 += 1;
                    }
                }
            }
        }
    }

    for (i = 0; i < x32; i++) {
        for (j = 0; j < 8; j++) {
            if (map->x32_map[i][j] != 0) {
                for (k = 0; k < 8; k++) {
                    if (map->x32_map[i][j] & (1 << k)) {
                        tpu_free->x32_used[tpu_free->x32] = (i * 64) + (j * 8) + k;
                        tpu_free->x32 += 1;
                    }
                }
            }
        }
    }
}

/** @brief Check if a CG number is already in the extended pattern cache. */
static s16 check_patcash_ex_trans(PatternCollection* padr, u32 cg) {
    s16 rnum = -1;
    s16 i;

    for (i = 0; i < padr->kazu; i++) {
        if (padr->adr[i]->cg.code == cg) {
            rnum = i;
            break;
        }
    }

    return rnum;
}

/** @brief Find a free pattern cache slot or evict the oldest entry. */
static s32 get_free_patcash_index(PatternCollection* padr) {
    s16 i;

    for (i = 0; i < 0x40; i++) {
        if (padr->patt[i].time == 0) {
            return i;
        }
    }

    flLogOut("ＣＧキャッシュバッファが一杯になりました。\n");
    // All slots occupied — evict the last one as a fallback to avoid OOB access
    return 0x3F;
}

/** @brief Decompress LZ-compressed 4bpp fixed-palette texture data. */
static void lz_ext_p6_fx(u8* srcptr, u8* dstptr, u32 len) {
    u8* endptr = dstptr + len;
    u8* tmpptr;
    u32 tmp;
    u32 flg;

    while (dstptr < endptr) {
        tmp = *srcptr++;

        switch (tmp & 0xC0) {
        case 0x0:
            *dstptr++ = tmp;
            break;

        case 0x40:
            tmp &= 0x3F;
            tmpptr = (dstptr - (tmp >> 2)) - 1;
            tmp = (tmp & 3) + 2;

            while (tmp--) {
                *dstptr++ = *tmpptr++;
            }

            break;

        case 0x80:
            tmp = ((tmp & 0x3F) << 8) | *srcptr++;
            tmpptr = (dstptr - (tmp >> 6)) - 1;
            tmp = (tmp & 0x3F) + 2;

            while (tmp--) {
                *dstptr++ = *tmpptr++;
            }

            break;

        case 0xC0:
            flg = tmp & 0x30;
            tmp = (tmp & 0xF) + 2;

            while (tmp--) {
                *dstptr++ = flg | (*srcptr >> 4);
                *dstptr++ = flg | (*srcptr++ & 0xF);
            }

            break;
        }
    }
}

/** @brief Decompress LZ-compressed paletted texture data with color lookup. */
static void lz_ext_p6_cx(u8* srcptr, u16* dstptr, u32 len, u16* palptr) {
    u16* endptr = dstptr + len;
    u16* tmpptr;
    u32 tmp;
    u32 flg;

    while (dstptr < endptr) {
        tmp = *srcptr++;

        switch (tmp & 0xC0) {
        case 0x0:
            *dstptr++ = palptr[tmp];
            break;

        case 0x40:
            tmp &= 0x3F;
            tmpptr = (dstptr - (tmp >> 2)) - 1;
            tmp = (tmp & 3) + 2;

            while (tmp--) {
                *dstptr++ = *tmpptr++;
            }

            break;

        case 0x80:
            tmp = ((tmp & 0x3F) << 8) | *srcptr++;
            tmpptr = (dstptr - (tmp >> 6)) - 1;
            tmp = (tmp & 0x3F) + 2;

            while (tmp--) {
                *dstptr++ = *tmpptr++;
            }

            break;

        case 0xC0:
            flg = tmp & 0x30;
            tmp = (tmp & 0xF) + 2;

            // ⚡ Bolt: 4× unroll — improves ILP for the palette gather pattern.
            // CPU can pipeline 4 independent load→index→store sequences.
            while (tmp >= 4) {
                dstptr[0] = palptr[flg | (srcptr[0] >> 4)];
                dstptr[1] = palptr[flg | (srcptr[0] & 0xF)];
                dstptr[2] = palptr[flg | (srcptr[1] >> 4)];
                dstptr[3] = palptr[flg | (srcptr[1] & 0xF)];
                dstptr[4] = palptr[flg | (srcptr[2] >> 4)];
                dstptr[5] = palptr[flg | (srcptr[2] & 0xF)];
                dstptr[6] = palptr[flg | (srcptr[3] >> 4)];
                dstptr[7] = palptr[flg | (srcptr[3] & 0xF)];
                srcptr += 4;
                dstptr += 8;
                tmp -= 4;
            }
            while (tmp--) {
                *dstptr++ = palptr[flg | (*srcptr >> 4)];
                *dstptr++ = palptr[flg | (*srcptr++ & 0xF)];
            }

            break;
        }
    }
}

/** @brief Initialize the multi-texture transformation system for a character. */
void mlt_obj_trans_init(MultiTexture* mt, s32 mode, u8* adrs) {
    PatternState* mc;
    PPGFileHeader ppg;
    s32 i;

    ppg.width = ppg.height = 16;
    ppg.compress = 0;
    ppg.formARGB = 0x1555;
    ppg.transNums = 0;
    mt->texList.tex = &mt->tex;

    switch (mode & 7) {
    case 4:
        ppg.pixel = 0x82;
        mt->texList.pal = NULL;
        break;

    case 2:
        ppg.pixel = 0x81;
        mt->texList.pal = palGetChunkGhostCP3();
        break;

    default:
        ppg.pixel = 0x81;
        mt->texList.pal = palGetChunkGhostDC();
        break;
    }

    mt->texList.tex->be = 0;
    ppgSetupTexChunkSeqs(&mt->tex, &ppg, adrs, mt->mltgidx16, mt->mltnum, mt->attribute);

    if (!(mode & 0x20)) {
        mc = mt->mltcsh16;

        for (i = 0; i < mt->mltnum16; i++) {
            mc->time = 0;
            mc->cs.code = -1;
            mc++;
        }

        mc = mt->mltcsh32;

        for (i = 0; i < mt->mltnum32; i++) {
            mc->time = 0;
            mc->cs.code = -1;
            mc++;
        }

        // ⚡ Bolt: memset replaces per-element loop — 0xFF fills both code (0xFFFFFFFF)
        // and slot (MLT_HASH_EMPTY = -1) correctly since both are all-bits-one sentinels.
        memset(s_hash16[mt->id], 0xFF, sizeof(s_hash16[0]));
        memset(s_hash32[mt->id], 0xFF, sizeof(s_hash32[0]));

    // `+"`u{26A1}`+" Opt4: Invalidate CG tile descriptor cache when texture groups are reloaded
    cg_cache_invalidate();
    }
}

/** @brief Update texture cache lifetimes for this multi-texture object. */
void mlt_obj_trans_update(MultiTexture* mt) {
    s32 i;
    PatternState* mc;

    PatternState* assign1;
    PatternState* assign2;

    for (mc = mt->mltcsh16, i = 0; i < mt->mltnum16; i++, mc += 1, assign1 = mc) {
        if (mc->time) {
            if (--mc->time == 0) {
                // ⚡ Invalidate hash entry for evicted tile
                u32 evict_code = mc->cs.code;
                MltHashEntry* ht16 = s_hash16[mt->id];
                u32 h = mlt_hash(evict_code);
                for (s32 p = 0; p < MLT_HASH_SIZE; p++) {
                    u32 idx = (h + p) & MLT_HASH_MASK;
                    if (ht16[idx].slot == MLT_HASH_EMPTY) break;
                    if (ht16[idx].code == evict_code && ht16[idx].slot == i) {
                        ht16[idx].slot = MLT_HASH_EMPTY;
                        ht16[idx].code = 0;
                        break;
                    }
                }
                mc->cs.code = -1;
            }
        }
    }

    for (mc = mt->mltcsh32, i = 0; i < mt->mltnum32; i++, mc += 1, assign2 = mc) {
        if (mc->time) {
            if (--mc->time == 0) {
                // ⚡ Invalidate hash entry for evicted tile
                u32 evict_code = mc->cs.code;
                MltHashEntry* ht32 = s_hash32[mt->id];
                u32 h = mlt_hash(evict_code);
                for (s32 p = 0; p < MLT_HASH_SIZE; p++) {
                    u32 idx = (h + p) & MLT_HASH_MASK;
                    if (ht32[idx].slot == MLT_HASH_EMPTY) break;
                    if (ht32[idx].code == evict_code && ht32[idx].slot == i) {
                        ht32[idx].slot = MLT_HASH_EMPTY;
                        ht32[idx].code = 0;
                        break;
                    }
                }
                mc->cs.code = -1U;
            }
        }
    }
}

/** @brief Draw a colored box primitive at the given coordinates. */
void draw_box(f64 arg0, f64 arg1, f64 arg2, f64 arg3, u32 col, u32 attr, s16 prio) {
    f32 px;
    f32 py;
    f32 sx;
    f32 sy;
    Vec3 point[2];
    PAL_CURSOR line;
    PAL_CURSOR_P xy[4];
    PAL_CURSOR_COL cc[4];

    px = arg0;
    py = arg1;
    sx = arg2;
    sy = arg3;
    point[0].x = px;
    point[0].y = py;
    point[0].z = 0.0f;
    point[1].x = px + sx;
    point[1].y = py + sy;
    point[1].z = 0.0f;
    njCalcPoints(NULL, point, point, 2);
    line.p = xy;
    line.col = cc;
    line.tex = NULL;
    line.num = 4;
    line.p[0].x = line.p[2].x = point[0].x;
    line.p[1].x = line.p[3].x = point[1].x;
    line.p[0].y = line.p[1].y = point[0].y;
    line.p[2].y = line.p[3].y = point[1].y;
    line.col[0].color = line.col[1].color = line.col[2].color = line.col[3].color = col;
    Renderer_Queue2DPrimitive((f32*)line.p, PrioBase[prio], (uintptr_t)line.col[0].color, 0);
    appRenewTempPriority(prio);
}

/** @brief Draw a debug line at the given position and size. */
static void DebugLine(f32 x, f32 y, f32 w, f32 h) {
    Vec3 point[2];
    PAL_CURSOR line;
    PAL_CURSOR_P xy[4];
    PAL_CURSOR_COL cc[4];

    line.p = &xy[0];
    line.col = &cc[0];
    line.tex = NULL;
    line.num = 4;
    point[0].x = x;
    point[0].y = y;
    point[0].z = 1.0f;
    point[1].x = x + w;
    point[1].y = y - h;
    point[1].z = 1.0f;
    njCalcPoints(NULL, point, point, 2);
    line.p[0].x = line.p[2].x = point[0].x;
    line.p[1].x = line.p[3].x = point[1].x;
    line.p[0].y = line.p[1].y = point[0].y;
    line.p[2].y = line.p[3].y = point[1].y;
    line.col[0].color = line.col[1].color = line.col[2].color = line.col[3].color = 0x80FFFFFF;
    Renderer_Queue2DPrimitive((f32*)line.p, PrioBase[1], (uintptr_t)line.col[0].color, 0);
}

/** @brief Melt (dissolve) a texture for transition effects. */
void mlt_obj_melt2(MultiTexture* mt, u16 cg_number) {
    u32* textbl;
    u16* trsbas;
    TileMapEntry* trsptr;
    TEX* texptr;
    TEX_GRP_LD* grplds;
    s32 count;
    s32 n;
    s32 i;
    s32 cd16;
    s32 cd32;
    s32 size;
    s32 attr;
    s32 palt;
    s32 wh;
    s32 dd;

    ppgSetupCurrentDataList(&mt->texList);
    grplds = &texgrplds[obj_group_table[cg_number]];

    if (grplds->ok == 0) {
        // The trans data is not valid. Group number: %d\n
        flLogOut("トランスデータが有効ではありません。グループ番号：%d\n", obj_group_table[cg_number]);
        return;
    }

    n = *(u32*)grplds->trans_table / 4;
    textbl = (u32*)grplds->texture_table;
    cd16 = 0;
    cd32 = 0;

    for (i = 0; i < n; i++) {
        trsbas = (u16*)(grplds->trans_table + ((u32*)grplds->trans_table)[i]);
        count = *trsbas;
        trsbas++;
        trsptr = (TileMapEntry*)trsbas;

        while (count != 0) {
            attr = trsptr->attr;

            if (!(attr & 0x1000)) {
                texptr = (TEX*)((uintptr_t)textbl + ((u32*)textbl)[trsptr->code]);
                dd = (((texptr->wh & 0xE0) << 5) - 0x400) | (((texptr->wh & 0x1C) << 6) - 0x100);
                wh = (texptr->wh & 3) + 1;
                size = (wh * wh) << 6;
                palt = attr & 3;

                switch (wh) {
                case 1:
                case 2:
                    lz77_gpu_or_cpu(&((u8*)texptr)[1], size * 2, size, mt, mt->mltgidx16, cd16, palt, 0);
                    attr = (attr & 0xC000) | 0x1000 | dd;
                    trsptr->attr |= 0x1000;
                    attr |= palt;
                    search_trsptr(grplds->trans_table, i, n, trsptr->code, palt, cd16, attr);
                    trsptr->code = cd16;
                    trsptr->attr = attr;
                    cd16 += 1;
                    break;

                case 4:
                    lz77_gpu_or_cpu(&((u8*)texptr)[1], size * 2, size, mt, mt->mltgidx32, cd32, palt, 1);
                    attr = (attr & 0xC000) | 0x3000 | dd;
                    trsptr->attr |= 0x1000;
                    attr |= palt;
                    search_trsptr(grplds->trans_table, i, n, trsptr->code, palt, cd32, attr);
                    trsptr->code = cd32;
                    trsptr->attr = attr;
                    cd32 += 1;
                    break;
                }
            }

            count -= 1;
            trsptr++;
        }
    }

    ppgRenewTexChunkSeqs(NULL);
}
