/**
 * @file feature_toggles.h
 * @brief Compile-time feature toggle stubs — RESIDUAL ONLY.
 *
 * Most subsystems now have self-guarding headers (#ifdef ENABLE_X in the
 * header itself). This file only covers subsystems whose headers don't yet
 * self-guard: controller_image, mods, librashader, netplay, training/Lua.
 *
 * Include this AFTER the feature's own header so the stubs override the
 * declarations only when the feature is disabled.
 */

#ifndef FEATURE_TOGGLES_H
#define FEATURE_TOGGLES_H

#include <stdbool.h>
#include <stddef.h>

/* ========================================================================= */
/* ENABLE_CONTROLLER_IMAGE                                                   */
/* ========================================================================= */
#ifndef ENABLE_CONTROLLER_IMAGE
static inline void ControllerImage_Init(void) {}
static inline void ControllerImage_Shutdown(void) {}
#endif

/* ========================================================================= */
/* ENABLE_MODS                                                               */
/* ========================================================================= */
#ifndef ENABLE_MODS
static inline void StageConfig_Init(void) {}
static inline void StageConfig_Load(int idx) { (void)idx; }
static inline void StageConfig_Save(int idx) { (void)idx; }
static inline void StageConfig_SetDefaultLayer(int idx) { (void)idx; }
static inline void modded_bgm_init(void) {}
static inline void modded_bgm_shutdown(void) {}
static inline void modded_stage_init(void) {}
static inline void modded_stage_shutdown(void) {}
#endif

/* ========================================================================= */
/* ENABLE_LIBRASHADER                                                        */
/* ========================================================================= */
#ifndef ENABLE_LIBRASHADER
static inline void SDLAppShaderConfig_Init(void) {}
static inline void SDLAppShaderConfig_Shutdown(void) {}
#endif

/* ========================================================================= */
/* ENABLE_NETPLAY                                                            */
/* ========================================================================= */
#ifndef ENABLE_NETPLAY
/* Core netplay */
static inline void Netplay_SetPlayerNumber(int p) { (void)p; }
static inline int  Netplay_GetPlayerNumber(void) { return 0; }
static inline void Netplay_SetRemoteIP(const char* ip) { (void)ip; }
static inline void Netplay_SetLocalPort(unsigned short p) { (void)p; }
static inline void Netplay_SetRemotePort(unsigned short p) { (void)p; }
static inline void Netplay_EnterLobby(void) {}
static inline void Netplay_Begin(void) {}
static inline void Netplay_Run(void) {}
static inline void Netplay_HandleMenuExit(void) {}
static inline bool Netplay_IsEnabled(void) { return false; }
static inline void Netplay_SetNegotiatedFT(int ft) { (void)ft; }
static inline int  Netplay_GetNegotiatedFT(void) { return 0; }
/* game_state.h netplay functions (called from engine code) */
static inline int  Netplay_GetPlayerHandle(void) { return 0; }
static inline int  Netplay_GetBattleStartFrame(void) { return 0; }
/* SDLNetplayUI_* stubs are in self-guarding sdl_netplay_ui.h */
#endif

/* ========================================================================= */
/* ENABLE_TRAINING                                                           */
/* ========================================================================= */
#ifndef ENABLE_TRAINING
static inline void lua_engine_bridge_init(void) {}
static inline void lua_engine_bridge_shutdown(void) {}
static inline void lua_engine_bridge_update(void) {}
static inline void lua_trials_loader_init(void) {}
static inline void lua_trials_loader_shutdown(void) {}
#endif

/* ========================================================================= */
/* Self-guarding headers handle these — NO stubs needed here:                */
/*   ENABLE_BROADCAST  -> broadcast.h                                        */
/*   ENABLE_BEZEL      -> sdl_app_bezel.h                                    */
/*   ENABLE_DEBUG_HUD  -> sdl_app_debug_hud.h                                */
/*   ENABLE_NETPLAY    -> sdl_netplay_ui.h                                   */
/*   ENABLE_RMLUI      -> all 38 rmlui_*.h headers                           */
/* ========================================================================= */

#endif /* FEATURE_TOGGLES_H */
