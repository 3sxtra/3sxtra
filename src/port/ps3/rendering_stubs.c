/**
 * @file rendering_stubs.c
 * @brief Minimal stubs for PS3 port — only functions NOT provided by AcrSDK/ps2/.
 *
 * The real AcrSDK/ps2/ files (foundaps2.c, flps2render.c, flps2vram.c,
 * flps2etc.c, flps2debug.c, flPADUSR.c) are now compiled directly.
 * This file only contains stubs for SDL-specific functions and features
 * disabled on PS3 (RmlUI, training, mods, netplay overlay textures, etc).
 */
#include "port/sdl/renderer/sdl_texture_util.h"
#include "port/rendering/renderer.h"
#include "port/sdl/app/sdl_app.h"

#include "port/input_definition.h"

void* TextureUtil_Load(const char* filename) { return 0; }
void* TextureUtil_LoadScaled(const char* filename, float scale) { return 0; }
void TextureUtil_Free(void* texture_id) {}
void TextureUtil_GetSize(void* texture_id, int* w, int* h) { if(w) *w = 0; if(h) *h = 0; }

extern void Renderer_SetTexture(int textureId);

void TextureUtil_DrawQuadEx(void* texture_id, float x, float y, float w, float h, float z, int flip_x, int flip_y) {
    if (!texture_id) return;
    uintptr_t tex_int = (uintptr_t)texture_id;
    Renderer_SetTexture(tex_int);

    RendererVertex v[4];
    v[0].x = x;     v[0].y = y;     v[0].z = z;  v[0].color = 0xFFFFFFFF; v[0].u = flip_x ? 1.0f : 0.0f; v[0].v = flip_y ? 1.0f : 0.0f;
    v[1].x = x + w; v[1].y = y;     v[1].z = z;  v[1].color = 0xFFFFFFFF; v[1].u = flip_x ? 0.0f : 1.0f; v[1].v = flip_y ? 1.0f : 0.0f;
    v[2].x = x;     v[2].y = y + h; v[2].z = z;  v[2].color = 0xFFFFFFFF; v[2].u = flip_x ? 1.0f : 0.0f; v[2].v = flip_y ? 0.0f : 1.0f;
    v[3].x = x + w; v[3].y = y + h; v[3].z = z;  v[3].color = 0xFFFFFFFF; v[3].u = flip_x ? 0.0f : 1.0f; v[3].v = flip_y ? 0.0f : 1.0f;
    Renderer_DrawTexturedQuadVtx(v, 4);
}

// Structural / Engine / Window stubs for PS3 port
int Renderer_LZ77Enqueue(const unsigned char* a, unsigned int b, unsigned int c, int d, int e, unsigned int f, unsigned int g) { return 0; }
void* LoadFullSpriteOverride(unsigned char a, unsigned short b, unsigned char c) { return 0; }

void LagtestRenderer_Render(void) {}

// Exposed by ps3_renderer_gcm.c
extern void CRS_Renderer_UpdateTexture(int textureId, const void* data, int x, int y, int width, int height);

void Renderer_UpdateTexture(int a, const void* b, int c, int d, int w, int h) {
    CRS_Renderer_UpdateTexture(a, b, c, d, w, h);
}

#include "port/ps3/app/ps3_app.h"

// SDL backend stubs redirected to PS3 native app layer
void SDLApp_BeginFrame(void) { PS3App_BeginFrame(); }
void SDLApp_EndFrame(void) { PS3App_EndFrame(); }
Uint64 SDLApp_GetTargetFrameTimeNS(void) { return PS3App_GetTargetFrameTimeNS(); }
SDL_Window* SDLApp_GetWindow(void) { return 0; }
int SDLApp_Init() { return PS3App_FullInit(); }
bool SDLApp_IsFrameRateUncapped(void) { return PS3App_IsFrameRateUncapped(); }
bool SDLApp_IsVSyncEnabled(void) { return PS3App_IsVSyncEnabled(); }
bool SDLApp_PollEvents() { return PS3App_PollEvents(); }
void SDLApp_PresentOnly(void) { PS3App_EndFrame(); }
void SDLApp_Quit(void) { PS3App_Quit(); }
void SDLApp_SetRenderer(RendererBackend r) {}
void SDLApp_SetWindowPosition(int x, int y) {}
void SDLApp_SetWindowSize(int w, int h) {}
#include "port/sdl/input/sdl_pad.h"

bool SDLPad_GetJoystickAxis(int a, int b, int sign) {
    if (!SDLPad_IsGamepadConnected(a)) return false;
    SDLPad_ButtonState state;
    SDLPad_GetButtonState(a, &state);
    int val = 0;
    switch (b) {
        case 0: val = state.left_stick_x; break;
        case 1: val = state.left_stick_y; break;
        case 2: val = state.right_stick_x; break;
        case 3: val = state.right_stick_y; break;
        case 4: val = state.left_trigger; break;
        case 5: val = state.right_trigger; break;
    }
    if (sign == 0) return val > 16000;
    else return val < -16000;
}
bool SDLPad_GetJoystickButton(int a, int b) {
    if (!SDLPad_IsGamepadConnected(a)) return false;
    SDLPad_ButtonState state;
    SDLPad_GetButtonState(a, &state);
    switch (b) {
        case 0: return state.south;
        case 1: return state.east;
        case 2: return state.west;
        case 3: return state.north;
        case 4: return state.back;
        case 6: return state.start;
        case 7: return state.left_stick;
        case 8: return state.right_stick;
        case 9: return state.left_shoulder;
        case 10: return state.right_shoulder;
        case 11: return state.dpad_up;
        case 12: return state.dpad_down;
        case 13: return state.dpad_left;
        case 14: return state.dpad_right;
    }
    return false;
}
bool SDLPad_GetJoystickHat(int a, int b, int dir) {
    if (!SDLPad_IsGamepadConnected(a)) return false;
    SDLPad_ButtonState state;
    SDLPad_GetButtonState(a, &state);
    if (dir == 0) return state.dpad_up;
    if (dir == 1) return state.dpad_right;
    if (dir == 2) return state.dpad_down;
    if (dir == 3) return state.dpad_left;
    return false;
}
bool SDLPad_IsJoystick(int a) { return SDLPad_IsGamepadConnected(a); }
bool SDLPad_IsKeyboard(int a) { return false; }
const unsigned char* SDL_GetKeyboardState(int* a) {
    static unsigned char kb[512] = {0};
    if(a) *a = 512;
    return kb;
}
void SDL_ShowOpenFolderDialog(void* a, void* b, void* c, void* d, void* e) {}

// Compression: zlibApp.c provides zlib_Decompress, zlib_InitSpurs, zlib_Initialize
// via edgeZlib SPURS (no stubs needed here)

// SPU/Testing/MenuBridge stubs
void MenuBridge_Init(void) {}
void MenuBridge_PostTick(void) {}
void MenuBridge_PreTick(int a) {}
int MenuBridge_StepGate(void) { return 0; }

// PS3 audio thread calls SPU_TickAudio(int16_t*, uint32_t) to mix SFX.
// Route to SPU_Tick() from spu.c, holding soundLock for thread safety.
#include <stdint.h>
extern void SPU_Tick(int16_t* output);
extern void* soundLock;
void SPU_TickAudio(int16_t* outbuf, uint32_t samples_per_channel) {
    SDL_LockMutex(soundLock);
    for (uint32_t i = 0; i < samples_per_channel; i++) {
        SPU_Tick(outbuf);
        outbuf += 2;
    }
    SDL_UnlockMutex(soundLock);
}
void TestRunner_Epilogue(void) {}
void TestRunner_Prologue(void) {}
bool is_joystick_input(InputID a) { return (a >= INPUT_ID_JOY_BASE); }
bool is_keyboard_input(InputID a) { return (a >= INPUT_ID_KEY_BASE && a < INPUT_ID_JOY_BASE); }


int g_resolution_scale = 1;

// UI/RmlUi Linker Stubs
int rmlui_casual_lobby_is_visible(void) { return 0; }
int rmlui_tournament_lobby_is_visible(void) { return 0; }
void rmlui_network_replay_picker_hide(void) {}
void rmlui_network_replay_picker_show(void) {}
void rmlui_network_replay_picker_poll(void) {}
void rmlui_network_replay_picker_update(void) {}
void rmlui_player_profile_hide(void) {}
void rmlui_player_profile_show(void) {}
void rmlui_player_profile_poll(void) {}
void rmlui_player_profile_update(void) {}
int g_lobby_reenter_from_match = 0;
int g_lobby_reenter_to_replay = 0;

// Training Mode Linker Stubs
void update_training_state(void) {}
void training_state_add_combo_hit(int a, int b) {}
void training_hud_draw(void) {}
void training_dummy_update_input(void* w) {}
void trials_init(void) {}
void trials_on_hit_registered(void) {}
int g_lua_dummy_active = 0;
short g_lua_dummy_player_id = 0;
void* g_dummy_settings = 0;
char g_training_state[1024] = {0}; // Dummy block to satisfy global struct linkage


// Modded Stage / UI Globals Linker Stubs
int game_paused = 0;
int mods_menu_fast_pre_game = 0;
