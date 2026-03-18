/**
 * @file stage_bg_registry.h
 * StageBackground Registry — named lifecycle callbacks for stage backgrounds.
 *
 * Replaces the hard-coded ta_move_tbl[22] dispatch table in tate00.c with a
 * self-registering registry of on_enter/on_tick callbacks per stage.
 */

#ifndef STAGE_BG_REGISTRY_H
#define STAGE_BG_REGISTRY_H

typedef enum {
    STAGE_BG_GILL = 0,  /* 00 bg000 – Gill (Final Boss, Greece)   */
    STAGE_BG_ALEX,      /* 01 bg010 – Alex (New York)             */
    STAGE_BG_RYU,       /* 02 bg020 – Ryu (Japan)                 */
    STAGE_BG_YUN,       /* 03 bg030 – Yun (Hong Kong)             */
    STAGE_BG_DUDLEY,    /* 04 bg040 – Dudley (England)            */
    STAGE_BG_NECRO,     /* 05 bg050 – Necro/Q (Russia)            */
    STAGE_BG_HUGO,      /* 06 bg060 – Hugo (Germany)              */
    STAGE_BG_IBUKI,     /* 07 bg070 – Ibuki (Japan)               */
    STAGE_BG_ELENA,     /* 08 bg080 – Elena (Kenya)               */
    STAGE_BG_ORO,       /* 09 bg090 – Oro (Brazil)                */
    STAGE_BG_YANG,      /* 10 bg100 – Yang (Hong Kong Alt)        */
    STAGE_BG_KEN,       /* 11 bg010 – Ken (New York Alt)          */
    STAGE_BG_SEAN,      /* 12 bg120 – Sean (Brazil)               */
    STAGE_BG_URIEN,     /* 13 bg130 – Urien (Egypt)               */
    STAGE_BG_AKUMA,     /* 14 bg140 – Akuma (Japan)               */
    STAGE_BG_CHUNLI,    /* 15 bg150 – Chun-Li (China)             */
    STAGE_BG_MAKOTO,    /* 16 bg160 – Makoto (Japan)              */
    STAGE_BG_NECRO_ALT, /* 17 bg180 – Q/Necro Alt                */
    STAGE_BG_TWELVE,    /* 18 bg180 – Twelve                      */
    STAGE_BG_REMY,      /* 19 bg190 – Remy                        */
    STAGE_BG_BONUS,     /* 20 Bonus Stage (Car)                   */
    STAGE_BG_BONUS2,    /* 21 Bonus Stage (Parry)                 */
    STAGE_BG_COUNT
} StageBgId;

typedef struct {
    void (*on_enter)(void); /* Called during init phases               */
    void (*on_tick)(void);  /* Called every frame during move phase    */
} StageBgCallbacks;

void StageBg_Register(StageBgId id, StageBgCallbacks callbacks);
const StageBgCallbacks* StageBg_Get(StageBgId id);

#endif
