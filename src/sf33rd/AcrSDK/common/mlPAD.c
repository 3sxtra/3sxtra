/**
 * @file mlPAD.c
 * @brief Middle-layer pad input — button remapping, stick direction, repeat keys.
 *
 * Reads raw pad data from the platform driver, applies lever/button
 * remapping via configurable tables, computes analog stick angles,
 * and generates repeat-key events for menu navigation.
 *
 * Part of the AcrSDK common module.
 * Originally from the PS2 SDK abstraction layer.
 */
#include "sf33rd/AcrSDK/common/mlPAD.h"
#include "common.h"
#include <stdbool.h>

#define PAD_COUNT 2
#define LEVER_FLIP_COUNT 4
#define PAD_BUTTON_COUNT 24
#define PAD_IO_MAP_SIZE 24
#define TWO_PI 6.2831855f
#define PAD_DEPTH_COUNT 16
#define PAD_PRESS_MAX 0xFF

volatile bool g_sim_lag_active = false;
int g_sim_lag_frame = 0;
#include "sf33rd/AcrSDK/ps2/flPADUSR.h"
#include "sf33rd/AcrSDK/ps2/ps2PAD.h"
#include "structs.h"

#include <SDL3/SDL.h>

#include <math.h>

const u8 fllever_flip_data[LEVER_FLIP_COUNT][16] = {
    { 0x00, 0x01, 0x02, 0x00, 0x04, 0x05, 0x06, 0x00, 0x08, 0x09, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x01, 0x02, 0x00, 0x08, 0x09, 0x0A, 0x00, 0x04, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x02, 0x01, 0x00, 0x04, 0x06, 0x05, 0x00, 0x08, 0x0A, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x02, 0x01, 0x00, 0x08, 0x0A, 0x09, 0x00, 0x04, 0x06, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 },
};

const u8 fllever_depth_flip_data[LEVER_FLIP_COUNT][4] = {
    { 0x00, 0x01, 0x02, 0x03 },
    { 0x00, 0x01, 0x03, 0x02 },
    { 0x01, 0x00, 0x02, 0x03 },
    { 0x01, 0x00, 0x03, 0x02 },
};

FLPAD* flpad_adr[PAD_COUNT];
FLPAD_CONFIG flpad_config[PAD_COUNT];
u8 NumOfValidPads;

FLPAD flpad_root[PAD_COUNT];
FLPAD flpad_conf[PAD_COUNT];

/** @brief Initialise the pad subsystem and configure default button mappings. */
s32 flPADInitialize() {
    s32 i;
    s32 flag = tarPADInit();

    flPADWorkClear();

    flpad_adr[0] = flpad_root;
    flpad_adr[1] = flpad_conf;

    for (i = 0; i < PAD_COUNT; i++) {
        flPADConfigSet(&fltpad_config_basic, i);
    }

    return flag;
}

/** @brief Shut down the pad subsystem. */
void flPADDestroy() {
    tarPADDestroy();
}

/** @brief Zero-clear both pad work areas (root and conf). */
void flPADWorkClear() {
    SDL_zeroa(flpad_root);
    SDL_zeroa(flpad_conf);
}

/** @brief Apply a pad configuration profile to a specific pad slot. */
void flPADConfigSet(const FLPAD_CONFIG* adrs, s32 padnum) {
    if ((u32)padnum >= PAD_COUNT) {
        return;
    }

    flpad_config[padnum] = *adrs;

    flPADConfigSetACRtoXX(
        padnum, flpad_config[padnum].abut_on, flpad_config[padnum].ast1_on, flpad_config[padnum].ast2_on);
}

/** @brief Read all pad inputs from the platform driver and apply button state tracking. */
void flPADGetALL() {
    s16 i;

    tarPADRead();
    NumOfValidPads = 0;

    for (i = 0; i < PAD_COUNT; i++) {
        flpad_adr[0][i].state = tarpad_root[i].state;
        flpad_adr[0][i].anstate = tarpad_root[i].anstate;
        flpad_adr[0][i].kind = tarpad_root[i].kind;
        flpad_adr[0][i].conn = tarpad_root[i].conn;

        if ((flpad_adr[0][i].kind != 0) && (flpad_adr[0][i].kind != 0x8000)) {
            NumOfValidPads += 1;
        }

        flpad_adr[0][i].stick[0] = tarpad_root[i].stick[0];
        flpad_adr[0][i].stick[1] = tarpad_root[i].stick[1];
        flpad_adr[0][i].anshot = tarpad_root[i].anshot;

        flupdate_pad_button_data(&flpad_adr[0][i], tarpad_root[i].sw);

        // ⚡ Bolt: Input Lag Test Injection
        if (g_sim_lag_active && i == 0) {
            // Inject Light Punch (SWK_SOUTH / Cross -> Mapped to LP by ioconv)
            flpad_adr[0][i].sw |= SWK_SOUTH;
            flpad_adr[0][i].sw_new |= SWK_SOUTH;

            // Log only on first frame of injection to avoid spam?
            // Or keep it for confirmation.
            // SDL_Log("Bolt: Inj Frame %d. SW: %08X", g_sim_lag_frame, flpad_adr[0][i].sw);
        }

        flupdate_pad_on_cnt(&flpad_adr[0][i]);
        flpad_adr[0][i].sw_repeat = flpad_adr[0][i].sw_new;
    }

    flPADACRConf();
}

/** @brief Apply button/lever remapping configuration to produce the conf pad data. */
void flPADACRConf() {
    u8* csh;
    u32 conf_data;
    u32 conf_data2;
    u32 st0;
    u32 ast1;
    u32 ast2;
    s16 i;
    s16 j;
    u8 depthflip[32];

    for (i = 0; i < PAD_COUNT; i++) {
        flpad_adr[1][i].state = flpad_adr[0][i].state;
        flpad_adr[1][i].anstate = flpad_adr[0][i].anstate;
        flpad_adr[1][i].kind = flpad_adr[0][i].kind;
        flpad_adr[1][i].conn = flpad_adr[0][i].conn;

        st0 = flpad_adr[0][i].sw & 0xF;
        ast1 = flpad_adr[0][i].sw >> 16 & 0xF;
        ast2 = flpad_adr[0][i].sw >> 20 & 0xF;
        conf_data = flpad_adr[0][i].sw & 0xFFF0;

        if (flpad_config[i].flip_lever < LEVER_FLIP_COUNT) {
            conf_data |= fllever_flip_data[flpad_config[i].flip_lever][st0];
        }
        if (flpad_config[i].flip_ast1 < LEVER_FLIP_COUNT) {
            conf_data |= fllever_flip_data[flpad_config[i].flip_ast1][ast1] << 0x10;
        }
        if (flpad_config[i].flip_ast2 < LEVER_FLIP_COUNT) {
            conf_data |= fllever_flip_data[flpad_config[i].flip_ast2][ast2] << 0x14;
        }

        csh = flpad_config[i].conf_sw;

        for (j = 0; j < 4; j++) {
            depthflip[j] = (flpad_config[i].flip_lever < LEVER_FLIP_COUNT)
                               ? flpad_adr[0][i].anshot.pow[fllever_depth_flip_data[flpad_config[i].flip_lever][j]]
                               : flpad_adr[0][i].anshot.pow[j];
        }

        for (j = 4; j < PAD_DEPTH_COUNT; j++) {
            depthflip[j] = flpad_adr[0][i].anshot.pow[j];
        }

        for (j = 0; j < PAD_DEPTH_COUNT; j++) {
            flpad_adr[1][i].anshot.pow[j] = 0;
        }

        conf_data2 = 0;

        for (j = 0; j < PAD_IO_MAP_SIZE; j++) {
            if (conf_data & flpad_io_map[j]) {
                if (csh[j] < 25) {
                    conf_data2 |= flpad_io_map[csh[j]];
                }
            }

            if (csh[j] < PAD_DEPTH_COUNT) {
                if (flpad_adr[1][i].anshot.pow[csh[j]] < depthflip[j]) {
                    flpad_adr[1][i].anshot.pow[csh[j]] = depthflip[j];
                }
            } else if (csh[j] > 0x18 && csh[j] < 25) {
                padconf_setup_depth(flpad_adr[1][i].anshot.pow, depthflip[j], flpad_io_map[csh[j]]);
            }
        }

        flupdate_pad_button_data(&flpad_adr[1][i], conf_data2);
        flupdate_pad_on_cnt(&flpad_adr[1][i]);

        flpad_adr[1][i].sw_repeat = flpad_adr[1][i].sw_new;
        flpad_adr[1][i].stick[0] = flpad_adr[0][i].stick[0];

        switch (flpad_config[i].flip_ast1) {
        case 1:
            flpad_adr[1][i].stick[0].x = flpad_adr[1][i].stick[0].x * -1;
            flpad_adr[1][i].stick[0].ang = 540 - flpad_adr[1][i].stick[0].ang;
            break;

        case 2:
            flpad_adr[1][i].stick[0].y = flpad_adr[1][i].stick[0].y * -1;
            flpad_adr[1][i].stick[0].ang = 360 - flpad_adr[1][i].stick[0].ang;
            break;

        case 3:
            flpad_adr[1][i].stick[0].x = flpad_adr[1][i].stick[0].x * -1;
            flpad_adr[1][i].stick[0].y = flpad_adr[1][i].stick[0].y * -1;
            flpad_adr[1][i].stick[0].ang = flpad_adr[1][i].stick[0].ang + 180;
            break;
        }

        flpad_adr[1][i].stick[1] = flpad_adr[0][i].stick[1];

        switch (flpad_config[i].flip_ast2) {
        case 1:
            flpad_adr[1][i].stick[1].x = flpad_adr[1][i].stick[1].x * -1;
            flpad_adr[1][i].stick[1].ang = 540 - flpad_adr[1][i].stick[1].ang;
            break;

        case 2:
            flpad_adr[1][i].stick[1].y = flpad_adr[1][i].stick[1].y * -1;
            flpad_adr[1][i].stick[1].ang = 360 - flpad_adr[1][i].stick[1].ang;
            break;

        case 3:
            flpad_adr[1][i].stick[1].x = flpad_adr[1][i].stick[1].x * -1;
            flpad_adr[1][i].stick[1].y = flpad_adr[1][i].stick[1].y * -1;
            flpad_adr[1][i].stick[1].ang = flpad_adr[1][i].stick[1].ang + 180;
            break;
        }

        flpad_adr[1][i].stick[0].ang = flpad_adr[1][i].stick[0].ang % 360;
        flpad_adr[1][i].stick[1].ang = flpad_adr[1][i].stick[1].ang % 360;

        flupdate_pad_stick_dir(&flpad_adr[1][i].stick[0]);
        flupdate_pad_stick_dir(&flpad_adr[1][i].stick[1]);
    }
}

/** @brief Set up analog depth values for multi-button mapping configurations. */
void padconf_setup_depth(u8* deps, u8 num, u32 iodat) {
    s32 i;

    for (i = 0; i < PAD_DEPTH_COUNT; i++) {
        if (iodat & flpad_io_map[i]) {
            if (deps[i] < num) {
                deps[i] = num;
            }

            if ((iodat ^= flpad_io_map[i]) == 0) {
                break;
            }
        }
    }
}

/** @brief Compute the radian angle for an analog stick from its x/y values. */
void flupdate_pad_stick_dir(PAD_STICK* st) {
    f32 radian;

    if ((st->y | st->x) == 0) {
        radian = 0.0f;
    } else {
        radian = atan2(-st->y, st->x);

        if (radian < 0.0f) {
            radian += TWO_PI;
        }
    }

    st->rad = radian;
}

/** @brief Update button edge-detection fields (new, off, chg) from raw switch data. */
void flupdate_pad_button_data(FLPAD* pad, u32 data) {
    pad->sw_old = pad->sw;
    pad->sw = data;
    pad->sw_new = pad->sw & (pad->sw_old ^ pad->sw);
    pad->sw_off = pad->sw_old & (pad->sw_old ^ pad->sw);
    pad->sw_chg = pad->sw_new | pad->sw_off;
}

/** @brief Increment press counters for all currently held buttons. */
void flupdate_pad_on_cnt(FLPAD* pad) {
    s16 i;

    for (i = 0; i < PAD_BUTTON_COUNT; i++) {
        if (pad->sw & flpad_io_map[i]) {
            if (pad->rpsw[i].ctr.press != PAD_PRESS_MAX) {
                pad->rpsw[i].ctr.press += 1;
            }
        } else {
            pad->rpsw[i].work = 0;
        }
    }
}

/** @brief Generate repeat-key events for held buttons based on timing parameters. */
void flPADSetRepeatSw(FLPAD* pad, u32 IOdata, u8 ctr, u8 times) {
    s32 i;
    u8 cmpctr;

    for (i = 0; i < PAD_BUTTON_COUNT; i++) {
        if (IOdata & flpad_io_map[i]) {
            if (pad->rpsw[i].ctr.sw_up >= times) {
                pad->rpsw[i].ctr.sw_up = times - 1;
            }

            cmpctr = ctr - pad->rpsw[i].ctr.sw_up * (ctr / times);

            if (pad->rpsw[i].ctr.press >= cmpctr) {
                pad->rpsw[i].ctr.press = 0;
                pad->rpsw[i].ctr.sw_up += 1;
                pad->sw_repeat |= flpad_io_map[i];
            }
        }
    }
}
