#include <stdint.h>
#include "types.h"
#include "structs.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "game_state.h"

typedef struct _TASK TASK;
#define TASK_MAX 32

// Dummy variables
int No_Trans;
u16 PLsw[2][2];
s16 exec_tm[8];
uintptr_t frw[EFFECT_MAX][448];
s16 frwctr;
s16 frwctr_min;
s16 frwque[EFFECT_MAX];
GameState g_GameState;
s16 head_ix[8];
u16 p1sw_0;
u16 p1sw_1;
u16 p1sw_buff;
u16 p2sw_0;
u16 p2sw_1;
u16 p2sw_buff;
s16 tail_ix[8];
TASK task[TASK_MAX];

// Dummy functions
void init_color_trans_req() {}
void init_texcash_before_process() {}
void seqsBeforeProcess() {}
void Game() {}
void seqsAfterProcess() {}
void texture_cash_update() {}
void move_pulpul_work() {}
void Check_LDREQ_Queue() {}
void cpExitTask(int task_id) {}
void grade_check_work_1st_init(int a, int b) {}
void Setup_Training_Difficulty() {}
void GameState_Save(void* dst) {}
void GameState_Load(const void* src) {}
