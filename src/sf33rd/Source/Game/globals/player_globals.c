/**
 * @file player_globals.c
 * @brief Player state global variable definitions.
 *
 * Player work areas, super art state, appearance, and per-player flags.
 * Split from game_globals.c for organizational clarity.
 */

#include "port/broadcast.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/spgauge.h"
#include "sf33rd/Source/Game/engine/stun.h"
#include "structs.h"
#include "types.h"

/* === Player State === */

PLW plw[2];
ZanzouTableEntry zanzou_table[2][48];
SA_WORK super_arts[2];
PiyoriType piyori_type[2];
AppearanceType appear_type;
s16 pcon_rno[4];
bool round_slow_flag;
bool pcon_dp_flag;
u8 win_sp_flag;
bool dead_voice_flag;
bool Scene_Cut;
bool Time_Over;

BroadcastConfig broadcast_config;
