/**
 * @file lua_engine_bridge.h
 * @brief C API for the Lua←→Engine bridge.
 *
 * Exposes native engine state (TrainingGameState, inputs, Lever_Buff) to the
 * RmlUi Lua environment so that FBNeo-style Lua scripts can run natively.
 */
#ifndef LUA_ENGINE_BRIDGE_H
#define LUA_ENGINE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the Lua engine bridge — registers the `engine` global table. */
void lua_engine_bridge_init(void);

/** Shutdown the Lua engine bridge. */
void lua_engine_bridge_shutdown(void);

/** Per-frame tick — fires emu._fire_before() callbacks in Lua. */
void lua_engine_bridge_tick(void);

/** Load the training_main.lua bootstrap (lazy, call once on first training frame). */
void lua_engine_bridge_load_training(void);

#ifdef __cplusplus
}
#endif

#endif /* LUA_ENGINE_BRIDGE_H */
