#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "structs.h"
#include "game_state.h"

// Mock the external global state
GameState g_state;

// Declaration of the function to test
extern void setupCharTableData(State* wk, s32 clr, s32 info);

// Stubs for undefined references in charset.c
void grade_add_same_move(s16 ix) { (void)ix; }
void pp_pulpara_remake_at_init(State* wk) { (void)wk; }
s16 random_16(void) { return 0; }
s16 hikusugi_check(State* wk) { (void)wk; return 0; }
s16 get_em_body_range(State* wk) { (void)wk; return 0; }
void setup_mvxy_data(State* wk, u16 ix) { (void)wk; (void)ix; }
void reset_mvxy_data(State* wk) { (void)wk; }
u32 random_32(void) { return 0; }
void pp_screen_quake(State* wk, s16 type) { (void)wk; (void)type; }
void setup_shell_hit_stop(State* wk, s16 tm, s16 fl) { (void)wk; (void)tm; (void)fl; }
void move_flag_clear_only_1(State* wk) { (void)wk; }
void grade_add_att_renew(State_Other* wk) { (void)wk; }
s16 cal_attdir(State* wk) { (void)wk; return 0; }
void add_sp_arts_gauge_init(State* wk, s16 val) { (void)wk; (void)val; }
bool ArcadeBalance_IsEnabled(void) { return false; }

// Dummy data tables
u16 chain_hidou_nm_air_table[1] = {0};
u16 chain_hidou_nm_ground_table[1] = {0};
u16 chain_normal_air_table[1] = {0};
u16 chain_normal_ground_table[1] = {0};
const u16 air_knockback_table[1] = {0};
const u16 ground_knockback_table[1] = {0};
void (*effinitjptbl[1])(State* wk, u8 eftype) = {0};
u8 plpat_rno_filter[1] = {0};
void (*sound_effect_request[1])(State* wk, u16 req) = {0};

void test_setupCharTableData_header_copy(void **state) {
    (void)state;

    State work;
    memset(&work, 0, sizeof(State));
    
    u32 mock_data[32]; 
    memset(mock_data, 0, sizeof(mock_data));

    // mock_data[2] is the anchor.
    // src[-1] = mock_data[1]
    // src[-2] = mock_data[0]
    mock_data[0] = 0xAABBCCDD; 
    mock_data[1] = 0x11223344; 
    mock_data[2] = 0x55667788; 

    work.set_char_ad = &mock_data[2]; 
    work.graphic_index = 0;
    work.char_graphic_data_type = 1; 

    // Test info=1 (Header Copy)
    setupCharTableData(&work, 0, 1);

    u32* dst_ptr = (u32*)&work.char_state.body.raw[0];
    (void)dst_ptr;
    
    // Check Header Block 2 (Offset -1)
    // Access via the new struct fields to verify they overlay correctly
    assert_int_equal(work.char_state.header_block_2, 0x11223344);
    
    // Check Header Block 1 (Offset -2)
    assert_int_equal(work.char_state.header_block_1, 0xAABBCCDD);
    
    // Body should NOT be copied when info=1
    assert_int_equal(work.char_state.body.raw[0], 0);
}

void test_setupCharTableData_body_copy(void **state) {
    (void)state;

    State work;
    memset(&work, 0, sizeof(State));
    
    u32 mock_data[32]; 
    memset(mock_data, 0, sizeof(mock_data));

    mock_data[0] = 0xAABBCCDD; 
    mock_data[1] = 0x11223344; 
    mock_data[2] = 0x55667788; 

    work.set_char_ad = &mock_data[2]; 
    work.graphic_index = 0;
    work.char_graphic_data_type = 1; 

    // Test info=0 (Body Copy)
    setupCharTableData(&work, 0, 0);

    // Body SHOULD be copied when info=0
    assert_int_equal(work.char_state.body.raw[0], 0x55667788);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_setupCharTableData_header_copy),
        cmocka_unit_test(test_setupCharTableData_body_copy),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
