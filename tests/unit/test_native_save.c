#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include "structs.h"

// Globals
struct _SAVE_W save_w[6];
struct _SYSTEM_W sys_w;
s16 bgm_level;
s16 se_level;
s32 X_Adjust;
s32 Y_Adjust;
u8 Disp_Size_H;
u8 Disp_Size_V;
s16 Present_Mode = 1; // Assuming 1 is a valid mode
SystemDir system_dir[6];
_REPLAY_W Replay_w;
struct _REP_GAME_INFOR Rep_Game_Infor[11];

// Mocks
s16 Check_SysDir_Page() {
    return 9;
}

const char* Paths_GetPrefPath(void) {
    return "./test_save_dir/";
}

void Copy_Save_w(void) {}
void Copy_Check_w(void) {}
void Save_Game_Data(void) {}
void setupSoundMode(void) {}
void SsBgmHalfVolume(s16 flag) { (void)flag; }
void setSeVolume(void) {}
void dspwhUnpack(u8 src, u8* xdsp, u8* ydsp) {
    *xdsp = 100 - ((src >> 4) & 0xF);
    *ydsp = 100 - (src & 0xF);
}

// Function to test
extern int NativeSave_LoadOptions(void);
extern void NativeSave_Init(void);

static int setup(void **state) {
    (void) state;
#ifdef _WIN32
    system("mkdir test_save_dir > NUL 2>&1");
#else
    system("mkdir -p test_save_dir");
#endif
    return 0;
}

static int teardown(void **state) {
    (void) state;
    // system("rm -rf test_save_dir");
    return 0;
}

static void test_load_options_not_found(void **state) {
    (void) state;
    
    // Ensure file doesn't exist
#ifdef _WIN32
    system("del test_save_dir\\options.ini > NUL 2>&1");
#else
    system("rm -f test_save_dir/options.ini");
#endif
    
    int result = NativeSave_LoadOptions();
    assert_int_equal(result, -1);
}

static void test_load_options_success(void **state) {
    (void) state;
    
    FILE *f = fopen("test_save_dir/options.ini", "w");
    assert_non_null(f);
    fprintf(f, "[Controller]\n");
    fprintf(f, "pad_1p_buttons=1,2,3,4,5,6,7,8\n");
    fprintf(f, "pad_1p_vibration=1\n");
    fprintf(f, "pad_2p_buttons=8,7,6,5,4,3,2,1\n");
    fprintf(f, "pad_2p_vibration=0\n");
    
    fprintf(f, "[Game]\n");
    fprintf(f, "difficulty=4\n");
    fprintf(f, "time_limit=99\n");
    fprintf(f, "battle_number_1=3\n");
    fprintf(f, "battle_number_2=1\n");
    fprintf(f, "damage_level=2\n");
    fprintf(f, "handicap=0\n");
    fprintf(f, "partner_type_1p=1\n");
    fprintf(f, "partner_type_2p=2\n");
    
    fprintf(f, "[Display]\n");
    fprintf(f, "adjust_x=-5\n");
    fprintf(f, "adjust_y=10\n");
    fprintf(f, "screen_size=50\n");
    fprintf(f, "screen_mode=1\n");
    
    fprintf(f, "[Gameplay]\n");
    fprintf(f, "guard_check=1\n");
    fprintf(f, "auto_save=1\n");
    fprintf(f, "analog_stick=0\n");
    fprintf(f, "unlock_all=1\n");
    
    fprintf(f, "[Sound]\n");
    fprintf(f, "bgm_type=1\n");
    fprintf(f, "sound_mode=0\n");
    fprintf(f, "bgm_level=12\n");
    fprintf(f, "se_level=14\n");
    
    fprintf(f, "[Extra]\n");
    fprintf(f, "extra_option=3\n");
    fprintf(f, "pl_color_1p=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19\n");
    fprintf(f, "pl_color_2p=19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0\n");
    
    fprintf(f, "extra_option_page_0=-1,0,1,2,3,4,5,6\n");
    
    fprintf(f, "[Broadcast]\n");
    fprintf(f, "broadcast_enabled=1\n");
    fprintf(f, "broadcast_source=2\n");
    fprintf(f, "broadcast_show_ui=1\n");
    
    fclose(f);
    
    int result = NativeSave_LoadOptions();
    assert_int_equal(result, 0);
    
    struct _SAVE_W* sw = &save_w[1]; // Present_Mode = 1
    
    // Check Controller
    // The load code puts the values at index 4 and 5 as well as 1:
    // memcpy(&save_w[4], sw, sizeof(struct _SAVE_W));
    // memcpy(&save_w[5], sw, sizeof(struct _SAVE_W));
    // Present_Mode = 1
    assert_int_equal(sw->Pad_Infor[0].Shot[0], 1);
    assert_int_equal(sw->Pad_Infor[0].Shot[7], 8);
    assert_int_equal(sw->Pad_Infor[0].Vibration, 1);
    
    assert_int_equal(sw->Pad_Infor[1].Shot[0], 8);
    assert_int_equal(sw->Pad_Infor[1].Shot[7], 1);
    assert_int_equal(sw->Pad_Infor[1].Vibration, 0);
    
    // Check Game
    assert_int_equal(sw->Difficulty, 4);
    assert_int_equal(sw->Time_Limit, 99);
    assert_int_equal(sw->Battle_Number[0], 3);
    assert_int_equal(sw->Battle_Number[1], 1);
    assert_int_equal(sw->Damage_Level, 2);
    assert_int_equal(sw->Handicap, 0);
    assert_int_equal(sw->Partner_Type[0], 1);
    assert_int_equal(sw->Partner_Type[1], 2);
    
    // Check Display
    assert_int_equal(sw->Adjust_X, -5);
    assert_int_equal(sw->Adjust_Y, 10);
    assert_int_equal(sw->Screen_Size, 50);
    assert_int_equal(sw->Screen_Mode, 1);
    
    // Check Gameplay
    assert_int_equal(sw->GuardCheck, 1);
    assert_int_equal(sw->Auto_Save, 1);
    assert_int_equal(sw->AnalogStick, 0);
    assert_int_equal(sw->Unlock_All, 1);
    
    // Check Sound
    assert_int_equal(sw->BgmType, 1);
    assert_int_equal(sw->SoundMode, 0);
    assert_int_equal(sw->BGM_Level, 12);
    assert_int_equal(sw->SE_Level, 14);
    
    // Check Extra
    assert_int_equal(sw->Extra_Option, 3);
    assert_int_equal(sw->PL_Color[0][0], 0);
    assert_int_equal(sw->PL_Color[0][19], 19);
    assert_int_equal(sw->PL_Color[1][0], 19);
    assert_int_equal(sw->PL_Color[1][19], 0);
    
    assert_int_equal(sw->extra_option.contents[0][0], -1);
    assert_int_equal(sw->extra_option.contents[0][7], 6);
    
    // Check Broadcast
    assert_int_equal(sw->broadcast_config.enabled, 1);
    assert_int_equal(sw->broadcast_config.source, 2);
    assert_int_equal(sw->broadcast_config.show_ui, 1);
    
    // Check globals updated
    assert_int_equal(sys_w.bgm_type, 1);
    assert_int_equal(sys_w.sound_mode, 0);
    assert_int_equal(bgm_level, 12);
    assert_int_equal(se_level, 14);
    assert_int_equal(X_Adjust, -5);
    assert_int_equal(Y_Adjust, 10);
    assert_int_equal(sys_w.screen_mode, 1);
    
    // Check dspwhUnpack side effects (50 -> 0x32)
    // 50 = dspwhPack(xdsp, ydsp) -> 100-ydsp | (100-xdsp)*16 = 50.
    // 50 in hex is 0x32.
    // Unpack: xdsp = 100 - (3), ydsp = 100 - (2).
    // So Disp_Size_H = 97, Disp_Size_V = 98.
    assert_int_equal(Disp_Size_H, 97);
    assert_int_equal(Disp_Size_V, 98);
}

#ifdef __cplusplus
}
#endif

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_load_options_not_found, setup, teardown),
        cmocka_unit_test_setup_teardown(test_load_options_success, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
