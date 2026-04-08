/**
 * @file bg_load.c
 * Background texture loading and resource management.
 * Split from bg.c — see SYSTEM_MODERNIZATION.md #37.
 */

#include "sf33rd/Source/Game/stage/bg_load.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/bg_rewrite.h"
#include <stdio.h> /* DEBUG: for ColorRAM dump */
#include "common.h"
#include "port/mods/modded_stage.h"
#include "port/renderer_plugin.h"
#include "port/sdl/renderer/sprite_override.h"
#include "port/sdl/renderer/sdl_texture_util.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/Source/Common/MemMan.h"
#include "sf33rd/Source/Common/PPGFile.h"
#include "sf33rd/Source/Common/PPGWork.h"
#include "sf33rd/Source/Game/ending/end_data.h"
#include "sf33rd/Source/Game/ending/end_maps.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include "sf33rd/Source/Game/system/ramcnt.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "structs.h"

int bg_texture_type = 0; // tracks ramcnt type: 0x12=gameplay, 0x18=select, etc

/** @brief Extract per-layer priority bytes from a packed u32 value. */
static void bg_extract_priorities(u32 prio_packed, u8 count) {
    u32 pmask = 0xFF000000;
    u8 shift = 0x18;
    u32 assign;
    u8 j;

    for (j = 0; j < count; j++, shift -= 8, assign = pmask >>= 8) {
        u32 prio = prio_packed & pmask;
        prio >>= shift;
        bg_priority[j] = prio;
    }
}

/** @brief Set up rewrite-screen texture chunks if needed. */
static u16 bg_setup_rewrite_textures(void* loadAdrs, u32 loadSize, u8 count, s32 base_gbix, u16 accnum) {
    u8 i;

    if (count == 0) {
        return accnum;
    }

    ppgSetupCurrentDataList(&ppgRwBgList);
    ppgSetupTexChunk_1st(NULL, loadAdrs, loadSize, base_gbix, count, 0, 0);
    ppgSetupTexChunk_1st_Accnum(0, accnum);

    for (i = 0; i < count; i++) {
        accnum = ppgSetupTexChunk_2nd(NULL, i + base_gbix);
        ppgSetupTexChunk_3rd(NULL, i + base_gbix, 1);
    }

    return accnum;
}

/** @brief Initialize background texture resources. */
void Bg_TexInit() {
    bg_texture_type = 0;
    if (RENDERER_HAS_PLUGIN()) {
        g_renderer_plugin->ClearBGTileCache();
        if (g_renderer_plugin->ClearTextureOverrideCache) {
            g_renderer_plugin->ClearTextureOverrideCache();
        }
    }
    ClearBGTileCache();
    s32 i;

    for (i = 0; i < 3; i++) {
        ppgBgList[i].tex = &ppgBgTex[i];
        ppgBgList[i].pal = palGetChunkGhostCP3();
    }

    ppgRwBgList.tex = &ppgRwBgTex;
    ppgRwBgList.pal = palGetChunkGhostCP3();
    ppgAkeList.tex = &ppgAkeTex;
    ppgAkeList.pal = &ppgAkePal;
    ppgAkaneList.tex = &ppgAkaneTex;
    ppgAkaneList.pal = &ppgAkanePal;
}

/** @brief Release background resources (textures, memory, PPG data). */
void Bg_Close() {
    u32 i;

    tokusyu_stage = 0;
    rw_num = 0;

    for (i = 0; i < 3; i++) {
        ppgReleaseTextureHandle(&ppgBgTex[i], -1);
    }

    ppgReleaseTextureHandle(&ppgRwBgTex, -1);
    ppgReleaseTextureHandle(&ppgAkeTex, -1);
    ppgReleasePaletteHandle(&ppgAkePal, -1);
    ppgReleaseTextureHandle(&ppgAkaneTex, -1);
    ppgReleasePaletteHandle(&ppgAkanePal, -1);
    Screen_Switch = 0;
    Screen_Switch_Buffer = 0;
    bg_disp_off = 0;

    /* Unload any modded stage textures */
    ModdedStage_Unload();
    s_gouki_pal_xored = 0;
}

/** @brief Load and configure background textures for the current stage. */
void Bg_Texture_Load_EX() {
    void* loadAdrs;
    u32 loadSize;
    u32 tgbix;
    u32 mask;
    s16 key1;
    u16 accnum;
    u8 i;
    u8 j;
    u8 stg;
    u8* akeAdrs;
    s32 akeSize;
    s16 akeKey;

    u32 assign2;
    u8 assign3;

    mmDebWriteTag("\nSTAGE\n\n");
    Bg_TexInit();
    bg_texture_type = 0x12; // gameplay stage (ramcnt type)

    for (i = 0; i < 8; i++) {
        bgPalCodeOffset[i] = 0x12C;
    }

    ending_flag = 0;

    for (stg = 0; stg < 3; stg++) {
        if (stage_bgw_number[bg_w.stage][stg] != 0) {
            break;
        }
    }

    for (i = 0; i < use_real_scr[bg_w.stage]; i++) {
        scr_bcm[stg + i] = bg_map_tbl[bg_w.stage][i];
    }

    for (i = 0; i < 3; i++) {
        if (stage_bgw_number[bg_w.stage][i] > 0) {
            Bg_On_R(1 << i);
        }
    }

    if (bg_w.stage == 7) {
        Bg_On_R(4);
    }

    key1 = Search_ramcnt_type(0x12);
    loadAdrs = (void*)Get_ramcnt_address(key1);
    loadSize = Get_size_data_ramcnt_key(key1);
    bg_extract_priorities(stage_priority[bg_w.stage], 3);

    bg_priority[3] = 70;
    accnum = 0;

    for (j = 0; j < bg_w.scrno; j++, assign3 = stg++) {
        tgbix = bgtex_stage_gbix[bg_w.stage][j];
        mask = 0x80000000;
        ppgSetupCurrentDataList(&ppgBgList[stg]);
        ppgSetupTexChunk_1st(NULL, loadAdrs, loadSize, (stg * 64) + 0x84, 32, 0, 0);
        ppgSetupTexChunk_1st_Accnum(0, accnum);

        for (i = 0; i < 32; i++, assign2 = mask >>= 1) {
            if (tgbix & mask) {
                accnum = ppgSetupTexChunk_2nd(NULL, i + ((stg * 64) + 0x84));
                ppgSetupTexChunk_3rd(NULL, i + ((stg * 64) + 0x84), 1);
            }
        }
    }

    accnum = bg_setup_rewrite_textures(loadAdrs, loadSize, rewrite_scr[bg_w.stage], (stg * 64) + 0x64, accnum);

    if (bg_w.stage == 7) {
        ppgSetupCurrentDataList(&ppgAkaneList);
        ppgSetupPalChunk(NULL, loadAdrs, loadSize, 0, 0, 1);
        ppgSetupTexChunk_1st(NULL, loadAdrs, loadSize, 0, 3, 0, 0);
        ppgSetupTexChunk_1st_Accnum(0, accnum);

        for (i = 0; i < 3; i++) {
            accnum = ppgSetupTexChunk_2nd(NULL, i);
            ppgSetupTexChunk_3rd(NULL, i, 1);
        }

        ppgSourceDataReleased(&ppgAkaneList);
    }

    if (bg_w.stage != 20 && bg_w.stage != 21) {
        akeKey = Search_ramcnt_type(0x1F);
        akeSize = Get_size_data_ramcnt_key(akeKey);
        akeAdrs = (u8*)Get_ramcnt_address(akeKey);
        ppgSetupCurrentDataList(&ppgAkeList);
        ppgSetupPalChunk(NULL, akeAdrs, akeSize, 0, 0, 1);
        ppgSetupTexChunk_1st(NULL, akeAdrs, akeSize, 0, 3, 0, 0);

        for (i = 0; i < 3; i++) {
            ppgSetupTexChunk_2nd(NULL, i);
            ppgSetupTexChunk_3rd(NULL, i, 1);
        }

        ppgSourceDataReleased(&ppgAkeList);
    }

    /* Try to load HD modded stage assets for this stage */
    ModdedStage_LoadForStage(bg_w.stage);

    /* Reset Shin Gouki palette state on stage load. */
    s_gouki_pal_xored = 0;
}

/** @brief Load background textures for a secondary display mode. */
void Bg_Texture_Load2(u8 type) {
    void* loadAdrs;
    u32 loadSize;
    s16 key;
    u32 tgbix;
    u32 prio;
    u32 mask;
    u32 pmask;
    u8 i;
    u8 j;
    u8 shift;

    u32 assign;

    mmDebWriteTag("\nBG ETC.\n\n");
    Bg_TexInit();
    bg_texture_type = 0x18; // select/etc screen (ramcnt type)
    (void)assign;
    ending_flag = 0;
    tokusyu_stage = 0;
    rw_num = 0;

    for (i = 0; i < 4; i++) {
        rw_bg_flag[i] = 0;
    }

    for (i = 0; i < bg_w.scno; i++) {
        scr_bcm[i] = bg_map_tbl2[type];
        Bg_On_R(1 << i);
    }

    ppgSetupCurrentDataList(ppgBgList);
    ppgReleaseTextureHandle(NULL, -1);
    key = Search_ramcnt_type(0x18);

    if (key == 0) {
        flLogOut("背景用テクスチャが読み込まれていませんでした。\n");
        while (!NULL) {};
    }

    loadSize = Get_size_data_ramcnt_key(key);
    loadAdrs = (void*)Get_ramcnt_address(key);
    ppgSetupTexChunk_1st(0, loadAdrs, loadSize, 0x84, 0x20, 0, 0);
    pmask = 0xFF000000;
    shift = 24;
    tgbix = bgtex_etc_gbix[type];
    mask = 0x80000000;
    prio = etc_bg_priority[type];
    prio &= pmask;
    prio >>= shift;
    bg_priority[0] = prio;

    for (j = 0, i = 0; i < 32; i++, assign = mask >>= 1) {
        if (tgbix & mask) {
            ppgBgList->tex->accnum = etcBgGixCnvTable[type][j];
            ppgSetupTexChunk_2nd(NULL, i + 0x84);
            ppgSetupTexChunk_3rd(NULL, i + 0x84, 1);
            j++;
        }
    }

    bgPalCodeOffset[0] = etcBgPalCnvTable[type] + 144;
}

/** @brief Load background textures used during ending sequences. */
void Bg_Texture_Load_Ending(s16 type) {
    void* loadAdrs;
    u32 loadSize;
    u16 accnum;
    u32 tgbix[2];
    u32 mask;
    s16 key1;
    u8 i;
    u8 j;
    u8 k;

    u32 assign2;

    mmDebWriteTag("\nENDING\n\n");
    rw_num = 0;
    Bg_TexInit();
    bg_texture_type = 0x20; // ending (distinct type)
    ending_flag = 1;

    for (i = 0; i < end_use_real_scr[type]; i++) {
        scr_bcm[i] = ending_map_tbl[type][i];
    }

    loadSize = load_it_use_any_key2(bgtex_ending_file[type], &loadAdrs, &key1, 2, 0);
    bg_extract_priorities(ending_priority[0], 4);

    for (accnum = 0, j = 0; j < bg_w.scrno; j++) {
        tgbix[0] = bgtex_ending_gbix[type][j * 2];
        tgbix[1] = bgtex_ending_gbix[type][(j * 2) + 1];
        mask = 0x80000000;
        ppgSetupCurrentDataList(&ppgBgList[j]);
        ppgSetupTexChunk_1st(NULL, loadAdrs, loadSize, (j * 64) + 100, 64, 0, 0);
        ppgSetupTexChunk_1st_Accnum(0, accnum);

        for (k = 0; k < 2; k++) {
            for (i = 0; i < 32; i++, assign2 = mask >>= 1) {
                if (mask & tgbix[k]) {
                    accnum = ppgSetupTexChunk_2nd(NULL, i + ((j * 64) + 100 + (k * 32)));
                    ppgSetupTexChunk_3rd(NULL, i + ((j * 64) + 100 + (k * 32)), 1);
                }
            }

            mask = 0x80000000;
        }
    }

    accnum = bg_setup_rewrite_textures(loadAdrs, loadSize, ending_rewrite_scr[type], (j * 64) + 100, accnum);

    switch (type) {
    case 14:
        tokusyu_stage = 5;

        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) {
                gouki_end_gbix[j + (i * 4)] = (j + ((i * 8) + 100));
            }
        }

        ppgSetupCurrentDataList(&ppgAkeList);
        ppgSetupPalChunk(NULL, loadAdrs, loadSize, 0, 0, 1);
        ppgSetupTexChunk_1st(NULL, loadAdrs, loadSize, 0x1A0, 0x18, 0, 0);
        ppgSetupTexChunk_1st_Accnum(0, accnum);

        for (i = 0; i < 0x18; i++) {
            accnum = ppgSetupTexChunk_2nd(NULL, i + 0x1A0);
            ppgSetupTexChunk_3rd(NULL, i + 0x1A0, 1);
        }

        break;

    case 15:
        tokusyu_stage = 6;
        break;

    case 19:
        tokusyu_stage = 7;
        ppgSetupCurrentDataList(&ppgAkeList);
        ppgSetupPalChunk(NULL, loadAdrs, loadSize, 0, 0, 1);
        ppgSetupTexChunk_1st(NULL, loadAdrs, loadSize, 0xE4, 1, 0, 0);
        ppgSetupTexChunk_1st_Accnum(0, accnum);
        accnum = ppgSetupTexChunk_2nd(NULL, 0xE4);
        ppgSetupTexChunk_3rd(NULL, 0xE4, 1);
        break;

    default:
        tokusyu_stage = 7;
        break;
    }

    Push_ramcnt_key(key1);
    Ed_Kakikae_Set(type);
    ppgSourceDataReleased(&ppgBgList[0]);
    ppgSourceDataReleased(&ppgBgList[1]);
    ppgSourceDataReleased(&ppgBgList[2]);
    ppgSourceDataReleased(&ppgRwBgList);
    ppgSourceDataReleased(&ppgAkeList);
}
