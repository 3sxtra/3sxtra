#include "port/sdl/renderer/sdl_texture_util.h"
#include "port/rendering/renderer.h"
#include "port/sdl/app/sdl_app.h"

void* TextureUtil_Load(const char* filename) { return 0; }
void* TextureUtil_LoadScaled(const char* filename, float scale) { return 0; }
void TextureUtil_Free(void* texture_id) {}
void TextureUtil_GetSize(void* texture_id, int* w, int* h) { if(w) *w = 0; if(h) *h = 0; }
void TextureUtil_DrawQuadEx(void* texture_id, float x, float y, float w, float h, float z, int flip_x, int flip_y) {}

void Renderer_SetTexture(int textureId) {}
void Renderer_DrawTexturedQuadVtx(const RendererVertex* vertices, int count) {}
void Renderer_Queue2DPrimitive(const float* pos, float priority, unsigned int data, int type) {}
void Renderer_DrawSpriteVtx(const RendererVertex* vertices, int count) {}
// Structural / Engine / Window stubs for PS3 port
int Renderer_LZ77Enqueue(const unsigned char* a, unsigned int b, unsigned int c, int d, int e, unsigned int f, unsigned int g) { return 0; }
void Renderer_UpdateTexture(int a, const void* b, int c, int d, int w, int h) {}
void* LoadFullSpriteOverride(unsigned char a, unsigned short b, unsigned char c) { return 0; }
void Renderer_Init(void) {}
void Renderer_DrawSolidQuadVtx(const RendererVertex* vertices, int count) {}
void Renderer_Flush2DPrimitives(void) {}
void Renderer_SetCurrentTexture(Texture* tex) {}
void SDLGameRenderer_FlushSprite2Batch(void) {}
void LagtestRenderer_Render(void) {}

// SDL backend stubs
void SDLApp_BeginFrame(void) {}
void SDLApp_EndFrame(void) {}
Uint64 SDLApp_GetTargetFrameTimeNS(void) { return 0; }
SDL_Window* SDLApp_GetWindow(void) { return 0; }
int SDLApp_Init() { return 0; }
bool SDLApp_IsFrameRateUncapped(void) { return false; }
bool SDLApp_IsVSyncEnabled(void) { return false; }
bool SDLApp_PollEvents() { return false; }
void SDLApp_PresentOnly(void) {}
void SDLApp_Quit(void) {}
void SDLApp_SetRenderer(RendererBackend r) {}
void SDLApp_SetWindowPosition(int x, int y) {}
void SDLApp_SetWindowSize(int w, int h) {}
short SDLPad_GetJoystickAxis(int a, int b) { return 0; }
int SDLPad_GetJoystickButton(int a, int b) { return 0; }
int SDLPad_GetJoystickHat(int a, int b) { return 0; }
int SDLPad_IsJoystick(int a) { return 0; }
int SDLPad_IsKeyboard(int a) { return 0; }
const unsigned char* SDL_GetKeyboardState(int* a) { if(a) *a = 0; return 0; }
void SDL_ShowOpenFolderDialog(void* a, void* b, void* c, void* d, void* e) {}

// Compression stubs
void zlib_Decompress(void* a, void* b, unsigned int c, unsigned int d) {}
void zlib_InitSpurs(void) {}
void zlib_Initialize(void) {}

// SPU/Testing/MenuBridge stubs
void MenuBridge_Init(void) {}
void MenuBridge_PostTick(void) {}
void MenuBridge_PreTick(int a) {}
int MenuBridge_StepGate(void) { return 0; }
void SPU_TickAudio(void) {}
void TestRunner_Epilogue(void) {}
void TestRunner_Prologue(void) {}
int is_joystick_input(int a) { return 0; }
int is_keyboard_input(int a) { return 0; }

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


// AcrSDK/PS2 hardware stubs for PS3 port
void* flAllocMemory(int size) { return 0; }
void flFlip(void) {}
int flPs2State[1024]; // Safe memory size for struct
int flPalette[1024]; 
void* flPS2GetSystemBuffAdrs(int handle) { return 0; }
int flTexture[1024];
int fltpad_config_basic[64];
int flpad_io_map[25] = {0};
int flCreatePaletteHandle(void) { return 0; }
void flReleasePaletteHandle(int handle) {}
int flCreateTextureHandle(void) { return 0; }
void flReleaseTextureHandle(int handle) {}
void* flLockTexture(int handle) { return 0; }
void flUnlockTexture(int handle) {}
void* flLockPalette(int handle) { return 0; }
void flUnlockPalette(int handle) {}
int flLogOut(const char* fmt, ...) { return 0; }
void flPrintL(int x, int y, const char* fmt, ...) {}
void flPrintColor(int color) {}
void flPS2ConvScreenFZ(int a, int b) {}
void flSetRenderState(int a, int b) {}
void flGetFrame(void) {}
void flGetSpace(void) {}
void flInitialize(void) {}

