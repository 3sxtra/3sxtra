#ifndef PLCNT_H
#define PLCNT_H

#include "structs.h"
#include "types.h"

#include <stdbool.h>

typedef struct {
    u8 normal_sa_graphic_ix;
    u8 ex_sa_graphic_ix;
    u8 ex_sa2_graphic_ix;
    u8 normal_sa_anim_ix;
    u8 ex_sa_anim_ix;
    u8 ex_sa2_anim_ix;
    u8 ex4th_full;
    s8 gauge_type;
    s16 gauge_length;
    s16 stock_max;
    s32 damage_time;
} SA_DATA;

typedef enum AppearanceType {
    APPEAR_TYPE_NON_ANIMATED, // Static appearance (no animation)
    APPEAR_TYPE_ANIMATED,     // Animated entrance (walk-in, etc.)
    APPEAR_TYPE_TRANSITIONAL, // Mid-round state transition (e.g., post-KO)
    APPEAR_TYPE_VICTORY,      // Victory sequence appearance
} AppearanceType;

extern const s8 plid_data[20];
extern const s16** kizetsu_timer_table[];

extern PlayerEntity plw[2];
extern SuperArtGauge super_arts[2];
extern ZanzouTableEntry afterimage_table[2][48];
extern StunState stun_state[2];
extern AppearanceType appear_type;
extern s16 pcon_rno[4];
extern bool round_slow_flag;
extern bool pcon_dp_flag;
extern u8 win_sp_flag;
extern bool dead_voice_flag;

// MARK: - Serialized

/// Afterimage data

/// Stun data

/// Player controller routine indices

/// `true` if the game has been slowed down at round end

/// `true` if death SFX playback needs to be requested

// MARK: - Unhandled

extern CollisionHurtbox rambod[2];
extern CollisionHurtboxExt ramhan[2];
extern u32 omop_spmv_ng_table[2];
extern u32 omop_spmv_ng_table2[2];
extern u16 vital_inc_timer;
extern u16 vital_dec_timer;
extern char cmd_sel[2];
extern s8 vib_sel[2];
extern s16 sag_inc_timer[2];
extern char no_sa[2];

void Player_control();
void reqPlayerDraw();
void erase_extra_plef_work();
void set_base_data_metamorphose(PlayerEntity* wk, s16 dmid);
void set_player_shadow(PlayerEntity* wk);
void clear_chainex_check(s16 ix);
void set_kizetsu_status(s16 ix);
void clear_kizetsu_point(PlayerEntity* wk);
void set_super_arts_status(s16 ix);
void clear_super_arts_point(PlayerEntity* wk);
s16 check_combo_end(s16 ix);
void set_quake(PlayerEntity* wk);
void add_next_position(PlayerEntity* wk);
void store_player_after_image_data();
void setup_base_and_other_data();
s32 check_sa_type_rebirth(PlayerEntity* wk);
void pli_0002();
void set_super_arts_status_dc(s16 ix);

#endif
