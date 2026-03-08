/**
 * @file mocks_stage_config.c
 * @brief Mock/stub dependencies for test_stage_config.c
 *
 * Provides stubs for Paths_GetBasePath() and the bg_data.h extern arrays
 * (use_real_scr, stage_bgw_number) that stage_config.c references.
 */
#include "port/config/paths.h"
#include "types.h"

/* --- Paths mock --- */

static const char* mock_base_path = "./";

const char* Paths_GetBasePath(void) {
    return mock_base_path;
}

/* Not used by stage_config.c but required by paths.h contract */
const char* Paths_GetPrefPath(void) {
    return "./test_stage_config_dir/";
}

int Paths_IsPortable(void) {
    return 0;
}

/* --- bg_data.h extern stubs --- */

/*
 * use_real_scr[stage] = number of real scroll layers for that stage.
 * Only indices 0-21 are valid. We fill a few with meaningful values for tests.
 */
const u8 use_real_scr[22] = {
    2, 1, 3, 2, 1, 2, 1, 2, 3, 1,
    2, 1, 2, 3, 1, 2, 1, 2, 1, 2,
    3, 1
};

/*
 * stage_bgw_number[stage][0..2] = BGW slot indices for that stage.
 * Only used by StageConfig_Load to determine foreground_bgw.
 */
const u8 stage_bgw_number[22][3] = {
    {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
    {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
    {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
    {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
    {0, 1, 2}, {0, 1, 2}
};
