# 3SX Naming Refactor Dictionary

This document records every variable/function rename performed during the semantic
modernization of the 3SX engine codebase. The original CPS3/arcade names are
preserved here as a permanent reference for anyone cross-referencing with the
original ROM, MAME source, or decompilation notes.

## GameState Struct Field Renames

### Phase Array Renames (FSM Control Variables)

| Original (CPS3) | New Name | Type | Purpose |
|------------------|----------|------|---------|
| `G_No[4]` | `fsm[4]` | `u8[4]` | Main FSM hierarchy: [0]=main_state, [1]=mode, [2]=sub, [3]=subsub |
| `E_No[4]` | `entry_phase[4]` | `u8[4]` | Entry/coin-up/join-in system phases |
| `D_No[4]` | `demo_phase[4]` | `u8[4]` | Attract-mode demo sequence phases |
| `S_No[4]` | `select_phase[4]` | `u8[4]` | Character select screen phases |
| `C_No[4]` | `manage_phase[4]` | `u8[4]` | In-match combat management phases |
| `M_No[4]` | `message_phase[4]` | `u8[4]` | Win/center message display phases |
| `SC_No[4]` | `next_cpu_phase[4]` | `u8[4]` | Next CPU opponent selection phases |
| `GO_No[4]` | `gameover_phase[4]` | `u8[4]` | Game over screen phases |
| `Cont_No[4]` | `continue_phase[4]` | `u8[4]` | Continue screen phases |

### Timer Renames

| Original | New Name | Type | Purpose |
|----------|----------|------|---------|
| `G_Timer` | `fsm_timer` | `s16` | General FSM transition timer |
| `E_Timer` | `entry_timer` | `s16` | Entry system timer |
| `D_Timer` | `demo_timer_global` | `s16` | Demo sequence timer |
| `S_Timer` | `select_timer_legacy` | `s16` | Select screen timer |
| `C_Timer` | `manage_timer` | `s16` | Combat management timer |
| `M_Timer` | `message_timer` | `s16` | Message display timer |

### Japanese → English Renames

| Original (Romaji) | New Name | Japanese | English Meaning |
|--------------------|----------|----------|-----------------|
| `ichikannkei` | `positional_relation` | 位置関係 | Positional/spatial relationship flag |
| `kakushi_ix` | `hidden_char_index` | 隠し | Hidden character selection index |
| `kakushi_op` | `hidden_char_operator` | 隠し | Hidden character operator ID |
| `tokusyu_stage` | `special_stage` | 特殊ステージ | Special/unique stage flag |
| `nosekae` | `palette_swap` | 乗せ替え | Palette swap/override |
| `y_sitei_pos` | `y_fixed_pos` | Y指定位置 | Fixed Y scroll position |
| `y_sitei_flag` | `y_fixed_flag` | Y指定フラグ | Fixed Y scroll flag |
| `c_kakikae` | `char_rewrite` | 書き換え | Character VRAM rewrite flag |
| `g_kakikae[2]` | `bg_rewrite[2]` | 書き換え | Background VRAM rewrite flag |
| `scrn_adgjust_y` | `screen_adjust_y` | — | Screen Y adjustment (typo fix) |
| `scrn_adgjust_x` | `screen_adjust_x` | — | Screen X adjustment (typo fix) |
| `aiuchi_flag` | `mutual_trade_flag` | 相打ち | Mutual/simultaneous KO flag |
| `paring_counter` | `parry_counter` | — | Parry attempt counter (spelling fix) |
| `paring_bonus_r` | `parry_bonus_r` | — | Parry bonus round counter |
| `paring_ctr_vs` | `parry_ctr_vs` | — | Per-match parry counter |
| `paring_ctr_ori` | `parry_ctr_ori` | — | Original/total parry counter |
| `paring_attack` | `parry_attack` | — | Parry-into-attack flag |
| `piyori_type` | `stun_type` | ピヨリ | Stun/dizzy animation type |
| `bs2_hosei[3]` | `bonus_stage2_offset[3]` | 補正 | Bonus stage 2 correction offset |

## Function Renames

### FSM Top-Level (game.c) — Completed in prior session

| Original | New Name |
|----------|----------|
| `Game00` | `Wait_Auto_Load` |
| `Game01` | `Game_CharSelect` |
| `Game02` | `Game_Fight` |
| `Game03` | `Game_WinResult` |
| `Game04` | `Game_LoseResult` |
| `Game05` | `Game_NextCPU` |
| `Game06` | `Game_GameOver` |
| `Game07` | `Game_Continue` |
| `Game08` | `Game_Ending` |
| `Game09` | `Game_Bonus` |
| `Game10` | `Game_AfterBonus` |
| `Game11` | `Game_NextQ` |
| `Game12` | `Game_Challenge` |

### Entry System (entry.c) — Completed in prior session

| Original | New Name |
|----------|----------|
| `Entry_00` | `Entry_TitleBlink` |
| `Entry_01` | `Entry_WaitStart` |
| `Entry_02` | `Entry_MidGameEntry` |
| `Entry_03` | `Entry_PreFightBreak` |
| `Entry_04` | `Entry_MidRoundBreak` |
| `Entry_06` | `Entry_PostContinueBreak` |
| `Entry_07` | `Entry_PostFightBreak` |
| `Entry_08` | `Entry_EndGameBreak` |
| `Entry_10` | `Entry_FinalEnding` |

### Demo System (demo02.c) — Completed in prior session

| Original | New Name |
|----------|----------|
| `Demo00` | `Demo_QuickStart` |
| `Demo01` | `Demo_FullAttract` |

### Ranking (ranking.c)

| Original | New Name |
|----------|----------|
| `Ranking_00` | `Ranking_Init` |
| `Ranking_01` | `Ranking_Display` |
