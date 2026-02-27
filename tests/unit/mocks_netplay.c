#include <stdint.h>
#include "types.h"
#include "structs.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "game_state.h"
#include "netplay/discovery.h"

typedef struct _TASK TASK;
#define TASK_MAX 11

// Dummy variables
u8 No_Trans;
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
void grade_check_work_1st_init(s16 a, s16 b) {}
void Setup_Training_Difficulty() {}
void GameState_Save(GameState* dst) {}
void GameState_Load(const GameState* src) {}

// Mocks for Discovery
void Discovery_Init(bool auto_connect) {}
void Discovery_SetReady(bool ready) {}
void Discovery_SetChallengeTarget(uint32_t instance_id) {}
int Discovery_GetChallengeTarget(void) { return 0; }
void Discovery_Update() {}
void Discovery_Shutdown() {}
uint32_t Discovery_GetLocalInstanceID(void) { return 1234; }
int Discovery_GetPeers(NetplayDiscoveredPeer* out_peers, int max_peers) { return 0; }

// Mocks for Config
bool Config_GetBool(const char* key) { return false; }
void SDLNetplayUI_SetNativeLobbyActive(bool active) {}

// Missing declarations
void Clear_Personal_Data(int p) {}
void Renderer_Flush2DPrimitives() {}
void SDLGameRenderer_ResetBatchState() {}
u16 Remap_Buttons(u16 inputs, const void* pad_infor) { return inputs; }
void Soft_Reset_Sub() {}
