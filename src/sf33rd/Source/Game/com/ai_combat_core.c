/**
 * @file ai_combat_core.c
 * @brief Data-driven Combat AI VM interpreter.
 *
 * Loads combat_sequences.dat at init, then dispatches AI subroutine calls
 * from binary pattern data at runtime. Replaces 20 hardcoded ai_active_*.c files.
 */

#include "sf33rd/Source/Game/com/ai_combat_core.h"
#include "game_state.h"
#include "sf33rd/Source/Game/com/ai_subroutines.h"
#include "port/config/paths.h"

#include <SDL3/SDL_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Binary format constants ---------- */
#define AIVM_MAGIC 0x4D564941 /* "AIVM" little-endian */
#define AIVM_VERSION 1
#define MAX_AI_CHARS 20
#define MAX_ARGS 16

/* ---------- Binary structures ---------- */
typedef struct {
    u16 char_id;
    u16 pattern_count;
    u32 data_offset;
} AICharDir;

typedef struct {
    u16 dim0;
    u16 dim1;
    u16 dim2;
    u32 data_offset;
} AITblDir;

/* ---------- Module state ---------- */
static u8* s_aivm_data = NULL;
static u32 s_aivm_size = 0;
static u16 s_num_chars = 0;
static AICharDir s_char_dir[MAX_AI_CHARS];

static u8* s_aitbl_data = NULL;
static u32 s_aitbl_size = 0;
static u16 s_num_tables = 0;
static AITblDir s_tbl_dir[16];

/* Cached pointers */
static const u8* s_char_pattern_data[MAX_AI_CHARS];
static const u8* s_tbl_data[16];

/* ---------- Helpers ---------- */

/** @brief Read a step's opcode, arg count, and args from the binary stream. */
static const u8* read_step(const u8* ptr, u8* out_op, u8* out_argc, s16 out_args[MAX_ARGS]) {
    *out_op = *ptr++;
    *out_argc = *ptr++;
    for (u8 i = 0; i < *out_argc && i < MAX_ARGS; i++) {
        out_args[i] = (s16)(ptr[0] | (ptr[1] << 8));
        ptr += 2;
    }
    return ptr;
}

/** @brief Skip N steps in the binary stream (to seek to a specific step index). */
static const u8* skip_steps(const u8* ptr, int count) {
    for (int i = 0; i < count; i++) {
        ptr++; /* opcode */
        u8 argc = *ptr++;
        ptr += argc * 2; /* args */
    }
    return ptr;
}

/** @brief Find the pattern data pointer for a character+pattern index. */
static const u8* find_pattern(int char_dir_idx, int pattern_idx) {
    if (char_dir_idx < 0 || char_dir_idx >= s_num_chars)
        return NULL;

    const u8* base = s_char_pattern_data[char_dir_idx];
    if (!base)
        return NULL;

    /* Walk through patterns sequentially to find the right one */
    const u8* ptr = base;
    for (int p = 0; p < pattern_idx; p++) {
        u8 step_count = *ptr++;
        ptr = skip_steps(ptr, step_count);
    }
    return ptr;
}

/** @brief Find the directory index for a character ID. */
static int find_char_dir_index(int char_id) {
    for (int i = 0; i < s_num_chars; i++) {
        if (s_char_dir[i].char_id == char_id)
            return i;
    }
    return -1;
}

/* ---------- Dispatch ---------- */

/** @brief Execute one AI subroutine call from opcode + args. */
static void dispatch_op(PlayerEntity* wk, u8 op, u8 argc, const s16* a) {
    switch (op) {
    case AI_OP_END_PATTERN:
        End_Pattern(wk);
        break;
    case AI_OP_LEVER_OFF:
        Lever_Off(wk);
        break;
    case AI_OP_LOOK:
        Look(wk, a[0]);
        break;
    case AI_OP_WAIT:
        Wait(wk, a[0]);
        break;
    case AI_OP_WALK:
        Walk(wk, (u16)a[0], a[1], a[2]);
        break;
    case AI_OP_JUMP:
        Jump(wk, a[0]);
        break;
    case AI_OP_HI_JUMP:
        Hi_Jump(wk, a[0], a[1]);
        break;
    case AI_OP_NORMAL_ATTACK:
        Normal_Attack(wk, a[0], (u16)a[1]);
        break;
    case AI_OP_NORMAL_ATTACK_SP:
        Normal_Attack_SP(wk, a[0], (u16)a[1], a[2]);
        break;
    case AI_OP_ADJUST_ATTACK:
        Adjust_Attack(wk, a[0], (u16)a[1]);
        break;
    case AI_OP_LEVER_ATTACK:
        Lever_Attack(wk, a[0], (u16)a[1], (u16)a[2]);
        break;
    case AI_OP_LEVER_ATTACK_SP:
        Lever_Attack_SP(wk, a[0], (u16)a[1], (u16)a[2], a[3]);
        break;
    case AI_OP_COMMAND_ATTACK:
        Command_Attack(wk, a[0], (u16)a[1], a[2], a[3]);
        break;
    case AI_OP_JUMP_ATTACK:
        Jump_Attack(wk, a[0], a[1], (u16)a[2], a[3]);
        break;
    case AI_OP_JUMP_COMMAND_ATTACK:
        Jump_Command_Attack(wk, a[0], (u16)a[1], a[2], a[3]);
        break;
    case AI_OP_RAPID_COMMAND_ATTACK:
        Rapid_Command_Attack(wk, a[0], (u16)a[1], a[2], (u16)a[3]);
        break;
    case AI_OP_JUMP_COMMAND_ATTACK_TERM:
        Jump_Command_Attack_Term(wk, a[0], (u16)a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], (u16)a[9]);
        break;
    case AI_OP_HI_JUMP_ATTACK:
        Hi_Jump_Attack(wk, a[0], a[1], (u16)a[2], a[3]);
        break;
    case AI_OP_HI_JUMP_ATTACK_TERM:
        Hi_Jump_Attack_Term(wk, a[0], a[1], a[2], (u16)a[3], a[4], a[5], a[6], (u16)a[7]);
        break;
    case AI_OP_HI_JUMP_COMMAND_ATTACK_TERM:
        Hi_Jump_Command_Attack_Term(wk, a[0], (u16)a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], (u16)a[9]);
        break;
    case AI_OP_CHECK_JUMP_ATTACK_CONDITIONS:
        Check_Jump_Attack_Conditions(wk, a[0], a[1], a[2], (u16)a[3], a[4], a[5], a[6], a[7]);
        break;
    case AI_OP_CHECK_ENEMY_DISTANCE:
        Check_Enemy_Distance(wk, a[0], a[1], a[2], a[3], a[4]);
        break;
    case AI_OP_APPROACH_WALK:
        Approach_Walk(wk, a[0], a[1]);
        break;
    case AI_OP_CHECK_SUPER_ART_CONDITIONS:
        Check_Super_Art_Conditions(wk, (u16)a[0], (u16)a[1], (u16)a[2], (u16)a[3]);
        break;
    case AI_OP_CHECK_SA:
        Check_SA(wk, a[0], a[1]);
        break;
    case AI_OP_CHECK_EX:
        Check_EX(wk, a[0], a[1]);
        break;
    case AI_OP_CHECK_SA_FULL:
        Check_SA_Full(wk, a[0], a[1]);
        break;
    case AI_OP_AI_RANDOM_ACTION_SELECT:
        AI_Random_Action_Select(wk, a[0], a[1], a[2], a[3], a[4], a[5]);
        break;
    case AI_OP_BRANCH_BY_DISTANCE:
        Branch_By_Distance(wk, a[0], a[1], a[2], a[3], a[4]);
        break;
    case AI_OP_ENABLE_OVERHEAD_ATTACK_FLAG:
        Enable_Overhead_Attack_Flag(wk);
        break;
    case AI_OP_LEVER_ON:
        Lever_On(wk, (u16)a[0], (u16)a[1]);
        break;
    case AI_OP_ONLY_SHOT:
        Only_Shot(wk, a[0]);
        break;
    case AI_OP_TURN_OVER_ON:
        Turn_Over_On(wk);
        break;
    case AI_OP_SETUP_DENJIN_LEVEL:
        Setup_DENJIN_LEVEL(wk);
        break;
    case AI_OP_HOLD_ATTACK_BUTTON:
        Hold_Attack_Button(wk, a[0]);
        break;
    case AI_OP_KEEP_AWAY:
        Keep_Away(wk, a[0], a[1]);
        break;
    case AI_OP_CHECK_SAFE_RETREAT_SPACE:
        Check_Safe_Retreat_Space(wk, a[0], a[1], a[2]);
        break;
    case AI_OP_PROVOKE:
        Provoke(wk, a[0]);
        break;
    case AI_OP_NEXT_ANOTHER_MENU:
        Next_Another_Menu(wk, a[0], (u16)a[1]);
        break;
    case AI_OP_CHECK_MISCELLANEOUS_CONDITIONS:
        Check_Miscellaneous_Conditions(wk, a[0], (u32)a[1], (u16)a[2]);
        break;
    case AI_OP_ORO_CHECK_JUMP_ATTACK:
        Oro_Check_Jump_Attack(wk, a[0], a[1], a[2], a[3], a[4], a[5], (u16)a[6], a[7], a[8], (u16)a[9]);
        break;
    case AI_OP_ORO_CHECK_HIGH_JUMP_ATTACK:
        Oro_Check_High_Jump_Attack(wk, a[0], a[1], a[2], a[3], a[4], a[5], (u16)a[6], a[7], a[8], (u16)a[9]);
        break;
    case AI_OP_ORO_CHECK_JUMP_COMMAND_ATTACK:
        Oro_Check_Jump_Command_Attack(
            wk, a[0], a[1], a[2], a[3], a[4], a[5], (u16)a[6], a[7], a[8], a[9], a[10], (u16)a[11]);
        break;
    case AI_OP_ORO_CHECK_HIGH_JUMP_COMMAND_ATTACK:
        Oro_Check_High_Jump_Command_Attack(
            wk, a[0], a[1], a[2], a[3], a[4], a[5], (u16)a[6], a[7], a[8], a[9], a[10], (u16)a[11]);
        break;
    default:
        SDL_Log("AI VM: unknown opcode %d for char %d", op, wk->wu.id);
        End_Pattern(wk);
        break;
    }
}

/* ---------- Public API ---------- */

void AICore_Init(void) {
    const char* base_path = Paths_GetBasePath();
    char path[1024];
    snprintf(path, sizeof(path), "%sassets/ai/combat_sequences.dat", base_path ? base_path : "");

    FILE* f = fopen(path, "rb");
    if (!f) {
        SDL_Log("AICore_Init: failed to open %s", path);
        return;
    }

    fseek(f, 0, SEEK_END);
    s_aivm_size = (u32)ftell(f);
    fseek(f, 0, SEEK_SET);

    s_aivm_data = (u8*)malloc(s_aivm_size);
    if (!s_aivm_data) {
        SDL_Log("AICore_Init: malloc failed (%u bytes)", s_aivm_size);
        fclose(f);
        return;
    }

    size_t read = fread(s_aivm_data, 1, s_aivm_size, f);
    fclose(f);

    if (read != s_aivm_size) {
        SDL_Log("AICore_Init: partial read (%zu / %u)", read, s_aivm_size);
        free(s_aivm_data);
        s_aivm_data = NULL;
        return;
    }

    /* Validate header */
    u32 magic = *(u32*)s_aivm_data;
    if (magic != AIVM_MAGIC) {
        SDL_Log("AICore_Init: bad magic 0x%08X", magic);
        free(s_aivm_data);
        s_aivm_data = NULL;
        return;
    }

    u16 version = *(u16*)(s_aivm_data + 4);
    s_num_chars = *(u16*)(s_aivm_data + 6);
    (void)version;

    if (s_num_chars > MAX_AI_CHARS)
        s_num_chars = MAX_AI_CHARS;

    /* Read character directory */
    const u8* dir_ptr = s_aivm_data + 8;
    for (int i = 0; i < s_num_chars; i++) {
        s_char_dir[i].char_id = *(u16*)(dir_ptr + 0);
        s_char_dir[i].pattern_count = *(u16*)(dir_ptr + 2);
        s_char_dir[i].data_offset = *(u32*)(dir_ptr + 4);
        s_char_pattern_data[i] = s_aivm_data + s_char_dir[i].data_offset;
        dir_ptr += 8;
    }

    SDL_Log("AICore_Init: loaded %u characters from %s (%u bytes)", s_num_chars, path, s_aivm_size);

    /* Load action_tables.dat */
    char tbl_path[1024];
    snprintf(tbl_path, sizeof(tbl_path), "%sassets/ai/action_tables.dat", base_path ? base_path : "");

    FILE* tf = fopen(tbl_path, "rb");
    if (!tf) {
        SDL_Log("AICore_Init: failed to open %s", tbl_path);
        return;
    }

    fseek(tf, 0, SEEK_END);
    s_aitbl_size = (u32)ftell(tf);
    fseek(tf, 0, SEEK_SET);

    s_aitbl_data = (u8*)malloc(s_aitbl_size);
    if (s_aitbl_data) {
        size_t tread = fread(s_aitbl_data, 1, s_aitbl_size, tf);
        if (tread == s_aitbl_size) {
            u32 tmagic = *(u32*)s_aitbl_data;
            if (tmagic == 0x54414941) { /* "AIAT" */
                s_num_tables = *(u16*)(s_aitbl_data + 6);
                if (s_num_tables > 16)
                    s_num_tables = 16;
                const u8* tdir_ptr = s_aitbl_data + 8;
                for (int i = 0; i < s_num_tables; i++) {
                    s_tbl_dir[i].dim0 = *(u16*)(tdir_ptr + 0);
                    s_tbl_dir[i].dim1 = *(u16*)(tdir_ptr + 2);
                    s_tbl_dir[i].dim2 = *(u16*)(tdir_ptr + 4);
                    s_tbl_dir[i].data_offset = *(u32*)(tdir_ptr + 6);
                    s_tbl_data[i] = s_aitbl_data + s_tbl_dir[i].data_offset;
                    tdir_ptr += 10;
                }
                SDL_Log("AICore_Init: loaded %u tables from %s (%u bytes)", s_num_tables, tbl_path, s_aitbl_size);
            } else {
                SDL_Log("AICore_Init: bad magic 0x%08X in tables", tmagic);
                free(s_aitbl_data);
                s_aitbl_data = NULL;
            }
        } else {
            free(s_aitbl_data);
            s_aitbl_data = NULL;
        }
    }
    fclose(tf);
}

void AICore_ExecutePattern(PlayerEntity* wk) {
    if (!s_aivm_data) {
        /* Fallback: do nothing if data not loaded */
        return;
    }

    /* player_number = character type (0-19), wu.id = player slot (0 or 1) */
    int char_type = wk->player_number;
    int player_slot = wk->wu.id;

    int dir_idx = find_char_dir_index(char_type);
    if (dir_idx < 0) {
        /* Character not in VM data — shouldn't happen */
        return;
    }

    /* Get current pattern index from game state (indexed by player slot) */
    s16 pattern_idx = (s16)g_state.Pattern_Index[player_slot];
    if (pattern_idx < 0 || pattern_idx >= s_char_dir[dir_idx].pattern_count) {
        End_Pattern(wk);
        return;
    }

    /* Navigate to the pattern */
    const u8* pat_ptr = find_pattern(dir_idx, pattern_idx);
    if (!pat_ptr) {
        End_Pattern(wk);
        return;
    }

    u8 step_count = *pat_ptr++;

    /* Get current step index from game state (indexed by player slot) */
    s16 step_idx = g_state.CP_Index[player_slot][0];
    if (step_idx < 0 || step_idx >= step_count) {
        /* Out of steps — end pattern (mirrors the 'default: End_Pattern' in original) */
        End_Pattern(wk);
        return;
    }

    /* Seek to the target step */
    const u8* step_ptr = skip_steps(pat_ptr, step_idx);

    /* Decode and dispatch */
    u8 op, argc;
    s16 args[MAX_ARGS] = { 0 };
    read_step(step_ptr, &op, &argc, args);
    dispatch_op(wk, op, argc, args);
}

void AICore_Shutdown(void) {
    if (s_aivm_data) {
        free(s_aivm_data);
        s_aivm_data = NULL;
    }
    if (s_aitbl_data) {
        free(s_aitbl_data);
        s_aitbl_data = NULL;
    }
    s_aivm_size = 0;
    s_num_chars = 0;
    s_aitbl_size = 0;
    s_num_tables = 0;
    memset(s_char_dir, 0, sizeof(s_char_dir));
    memset(s_char_pattern_data, 0, sizeof(s_char_pattern_data));
    memset(s_tbl_dir, 0, sizeof(s_tbl_dir));
    memset(s_tbl_data, 0, sizeof(s_tbl_data));
}

u8 AICore_GetActionTableValue(AIActionTableType table, int dim0, int dim1, int dim2) {
    int tbl_idx = (int)table;
    if (tbl_idx < 0 || tbl_idx >= s_num_tables || !s_aitbl_data) {
        return 0;
    }
    const AITblDir* dir = &s_tbl_dir[tbl_idx];
    if (dim0 < 0 || dim0 >= dir->dim0)
        dim0 = 0;
    if (dim1 < 0 || dim1 >= dir->dim1)
        dim1 = 0;
    if (dim2 < 0 || dim2 >= dir->dim2)
        dim2 = 0;

    int flat_idx = (dim0 * dir->dim1 * dir->dim2) + (dim1 * dir->dim2) + dim2;
    return s_tbl_data[tbl_idx][flat_idx];
}
