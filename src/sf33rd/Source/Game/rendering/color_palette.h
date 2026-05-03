/**
 * @file color3rd.h
 * @brief Color palette loading, conversion, and ghost palette declarations.
 */

#ifndef COLOR_PALETTE_H
#define COLOR_PALETTE_H

#include "structs.h"
#include "types.h"

extern u16 ColorRAM[512][64];
extern Col3rd_W col3rd_w;

/** CPS3 character color palette archive — two banks of 28 sub-palettes, 64 colors each. */
typedef struct {
    u16 col[2][28][64];
} COL;

/** Per-player color palette archive pointer (set by init_trans_color_ram). */
extern COL* plcol[2];

void q_ldreq_color_data(REQ* curr);
void load_any_color(u16 ix, u8 kokey);
void set_hitmark_color();
void init_trans_color_ram(s16 id, s16 key, u8 type, u16 data);
void init_color_trans_req();
void push_color_trans_req(s16 from_col, s16 to_col);
void palCopyGhostDC(s32 ofs, s32 cnt, void* data);
u16 palConvSrcToRam(u16 col);
void palCreateGhost();
Palette* palGetChunkGhostDC();
Palette* palGetChunkGhostCP3();
void palUpdateGhostDC();
void palUpdateGhostCP3(s32 pal, s32 nums);
void palConvRowTim2CI8Clut(u16* src, u16* dst, s32 size);

/** Register a callback to receive palette source data before it is freed.
 *  The palmod editor uses this to cache a persistent copy of the COL data
 *  so it can safely re-apply palettes at any time without reading freed memory. */
void palmod_set_hook(void (*hook)(int id, const COL* data));

#endif
