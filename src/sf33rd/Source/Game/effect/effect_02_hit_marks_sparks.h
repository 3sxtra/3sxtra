#ifndef EFFECT_02_HIT_MARKS_SPARKS_H
#define EFFECT_02_HIT_MARKS_SPARKS_H

#include "structs.h"
#include "types.h"

typedef struct {
    u16 se;
    u8 hits;
    u8 deff;
    u8 kezu;
    u8 fsin;
    u8 status;
    u8 quake;
    u8 dir;
    u8 col;
    u8 myhix;
    u8 emhix;
} HMDT;

typedef struct {
    u16 chix;
    s16 hx;
    s16 hy;
} EXPLEM;

extern const s16 hit_mark_dir_table[16];
extern const HMDT hmdt[];
extern const s16 hcct[];
extern const s16 gqdt[][2];
extern const EXPLEM explem[];
extern const EXPLEM explem2[][20];
extern const s16 hit_mark_adjust_table[][2];

void effect_02_move(State_Other* ewk);
s32 effect_02_init(State* wk, s8 dmgp, s8 mkst, s8 dmrl);

#endif
