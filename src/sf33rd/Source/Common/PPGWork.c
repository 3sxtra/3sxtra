/**
 * @file PPGWork.c
 * @brief PPG work area globals and VRAM purge helpers.
 *
 * Global texture, palette, and data-list instances for backgrounds,
 * screens, menus, the opening sequence, and the Capcom logo. Also
 * provides per-category VRAM purge functions to release handles.
 *
 * Part of the Common module.
 * Originally from the PS2 PPG work-area module.
 */
#include "sf33rd/Source/Common/PPGWork.h"
#include "common.h"
#include "sf33rd/AcrSDK/ps2/flps2vram.h"
#include "sf33rd/Source/Game/rendering/aboutspr.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"

#define PPG_BG_COUNT 4    /**< Number of background texture/list slots. */
#define MTS_SLOT_COUNT 24 /**< Number of per-character metamorphose texture slots. */

// sbss

Texture ppgBgTex[PPG_BG_COUNT];
PPGDataList ppgBgList[PPG_BG_COUNT];

Texture ppgRwBgTex;
PPGDataList ppgRwBgList;

Texture ppgAkeTex;
Palette ppgAkePal;
PPGDataList ppgAkeList;

Texture ppgAkaneTex;
Palette ppgAkanePal;
PPGDataList ppgAkaneList;

Texture ppgScrTex;
Palette ppgScrPal;
Palette ppgScrPalFace;
Palette ppgScrPalShot;
Palette ppgScrPalOpt;
PPGDataList ppgScrList;
PPGDataList ppgScrListFace;
PPGDataList ppgScrListShot;
PPGDataList ppgScrListOpt;

Texture ppgOpnBgTex;
PPGDataList ppgOpnBgList;

Texture ppgTitleTex;
PPGDataList ppgTitleList;

Texture ppgWarTex;
Palette ppgWarPal;
Palette ppgAdxPal;
PPGDataList ppgWarList;
PPGDataList ppgAdxList;

Texture ppgCapLogoTex;
Palette ppgCapLogoPal;
PPGDataList ppgCapLogoList;

/** @brief Reset all PPG work area texture/palette instances to uninitialised. */
void ppgWorkInitializeApprication() {
    s32 i;

    for (i = 0; i < PPG_BG_COUNT; i++) {
        ppgBgTex[i].be = 0;
    }

    ppgRwBgTex.be = 0;
    ppgAkeTex.be = ppgAkePal.be = 0;
    ppgAkaneTex.be = ppgAkanePal.be = 0;
    ppgScrTex.be = ppgScrPal.be = ppgScrPalFace.be = ppgScrPalShot.be = ppgScrPalOpt.be = 0;
    ppgOpnBgTex.be = 0;
    ppgTitleTex.be = 0;
    ppgWarTex.be = ppgWarPal.be = ppgAdxPal.be = 0;
    ppgCapLogoTex.be = ppgCapLogoPal.be = 0;

    for (i = 0; i < MTS_SLOT_COUNT; i++) {
        mts[i].tex.be = 0;
    }

    col3rd_w.palDC.be = col3rd_w.palCP3.be = 0;
}

/** @brief Purge textures and palettes from VRAM by category type. */
void ppgPurgeFromVRAM(s32 type) {
    s32 i;

    switch (type) {
    case 0:
        if (ppgScrTex.be) {
            ppgPurgeTextureFromVRAM(&ppgScrTex);
        }

        if (ppgScrPal.be) {
            ppgPurgePaletteFromVRAM(&ppgScrPal);
        }

        if (ppgScrPalFace.be) {
            ppgPurgePaletteFromVRAM(&ppgScrPalFace);
        }

        if (ppgScrPalShot.be) {
            ppgPurgePaletteFromVRAM(&ppgScrPalShot);
        }

        if (ppgScrPalOpt.be) {
            ppgPurgePaletteFromVRAM(&ppgScrPalOpt);
        }

        break;

    case 1:
        if (ppgWarTex.be) {
            ppgPurgeTextureFromVRAM(&ppgWarTex);
        }

        if (ppgWarPal.be) {
            ppgPurgePaletteFromVRAM(&ppgWarPal);
        }

        if (ppgAdxPal.be) {
            ppgPurgePaletteFromVRAM(&ppgAdxPal);
        }

        break;

    case 2:
        if (ppgOpnBgTex.be) {
            ppgPurgeTextureFromVRAM(&ppgOpnBgTex);
        }

        if (ppgCapLogoTex.be) {
            ppgPurgeTextureFromVRAM(&ppgCapLogoTex);
        }

        if (ppgCapLogoPal.be) {
            ppgPurgePaletteFromVRAM(&ppgCapLogoPal);
        }

        if (col3rd_w.palDC.be) {
            ppgPurgePaletteFromVRAM(&col3rd_w.palDC);
        }

        if (col3rd_w.palCP3.be) {
            ppgPurgePaletteFromVRAM(&col3rd_w.palCP3);
        }

        break;

    case 3:
        if (ppgTitleTex.be) {
            ppgPurgeTextureFromVRAM(&ppgTitleTex);
        }

        break;

    case 4:
        for (i = 0; i < PPG_BG_COUNT; i++) {
            if (ppgBgTex[i].be) {
                ppgPurgeTextureFromVRAM(&ppgBgTex[i]);
            }
        }

        if (col3rd_w.palDC.be) {
            ppgPurgePaletteFromVRAM(&col3rd_w.palDC);
        }

        if (col3rd_w.palCP3.be) {
            ppgPurgePaletteFromVRAM(&col3rd_w.palCP3);
        }

        if (ppgAkeTex.be) {
            ppgPurgeTextureFromVRAM(&ppgAkeTex);
        }

        if (ppgAkaneTex.be) {
            ppgPurgeTextureFromVRAM(&ppgAkaneTex);
        }

        if (ppgAkePal.be) {
            ppgPurgePaletteFromVRAM(&ppgAkePal);
        }

        if (ppgAkanePal.be) {
            ppgPurgePaletteFromVRAM(&ppgAkanePal);
        }

        break;

    case 5:
        for (i = 1; i < MTS_SLOT_COUNT; i++) {
            if ((mts_ok[i].be) && (mts[i].tex.be)) {
                ppgPurgeTextureFromVRAM(&mts[i].tex);
            }
        }

        if (col3rd_w.palDC.be) {
            ppgPurgePaletteFromVRAM(&col3rd_w.palDC);
        }

        if (col3rd_w.palCP3.be) {
            ppgPurgePaletteFromVRAM(&col3rd_w.palCP3);
        }

        break;
    }
}

/** @brief Release all texture handles for the given texture object. */
void ppgPurgeTextureFromVRAM(Texture* tex) {
    (void)tex;
}

/** @brief Release all palette handles for the given palette object. */
void ppgPurgePaletteFromVRAM(Palette* pal) {
    (void)pal;
}
